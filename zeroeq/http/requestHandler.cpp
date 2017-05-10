
/* Copyright (c) 2016-2017, Human Brain Project
 *                          Stefan.Eilemann@epfl.ch
 *                          Daniel.Nachbaur@epfl.ch
 */

#include "requestHandler.h"

#include <zeroeq/log.h>
#include <zeroeq/uri.h>

#include <future>
#include <memory> // shared_from_this
#include <stdexcept>
#include <zmq.h>

namespace zeroeq
{
namespace http
{
namespace
{
int _getContentLength(const HTTPServer::request& request)
{
    for (const auto& i : request.headers)
    {
        if (i.name == "Content-Length")
            return std::stoi(i.value);
    }
    return 0;
}

Method _getMethodType(const std::string& methodName)
{
    if (methodName == "GET")
        return Method::GET;
    if (methodName == "POST")
        return Method::POST;
    if (methodName == "PUT")
        return Method::PUT;
    if (methodName == "PATCH")
        return Method::PATCH;
    if (methodName == "DELETE")
        return Method::DELETE;
    throw std::invalid_argument("Method not supported");
}

std::string _headerEnumToString(const Header header)
{
    switch (header)
    {
    case Header::ALLOW:
        return "Allow";
    case Header::CONTENT_TYPE:
        return "Content-Type";
    case Header::LAST_MODIFIED:
        return "Last-Modified";
    case Header::LOCATION:
        return "Location";
    case Header::RETRY_AFTER:
        return "Retry-After";
    default:
        throw std::logic_error("no such header");
    }
}

// The actual handler for each incoming request where the data is read from
// a dedicated connection to the client.
struct ConnectionHandler : std::enable_shared_from_this<ConnectionHandler>
{
    ConnectionHandler(const HTTPServer::request& request, void* socket)
        : _request(request)
        , _socket(socket)
    {
    }

    void operator()(HTTPServer::connection_ptr connection)
    {
        try
        {
            const auto method = _getMethodType(_request.method);
            if (method != Method::GET)
            {
                _size = _getContentLength(_request);
                // if we have payload, schedule an (async) read of all chunks.
                // Will call _handleRequest() after all data has been read.
                if (_size > 0)
                {
                    _readChunk(connection, method);
                    return;
                }
            }
            _handleRequest(method, connection);
        }
        catch (const std::invalid_argument&)
        {
            std::vector<HTTPServer::response_header> headers;
            _addCorsHeaders(headers);
            connection->set_status(HTTPServer::connection::not_supported);
            connection->set_headers(headers);
        }
    }

private:
    void _readChunk(HTTPServer::connection_ptr connection, const Method method)
    {
        namespace pl = std::placeholders;
        connection->read(std::bind(&ConnectionHandler::_handleChunk,
                                   ConnectionHandler::shared_from_this(),
                                   pl::_1, pl::_2, pl::_3, connection, method));
    }

    void _handleChunk(HTTPServer::connection::input_range range,
                      const boost::system::error_code error, const size_t size,
                      HTTPServer::connection_ptr connection,
                      const Method method_)
    {
        if (error)
        {
            ZEROEQERROR << "Error during ConnectionHandler::_handleChunk: "
                        << error.message() << std::endl;
            return;
        }

        _body.append(std::begin(range), size);
        _size -= size;
        if (_size > 0)
            _readChunk(connection, method_);
        else
            _handleRequest(method_, connection);
    }

    void _handleRequest(const Method method,
                        HTTPServer::connection_ptr connection)
    {
        Message message;
        message.request.method = method;
        const auto uri = URI(_request.destination);
        message.request.path = uri.getPath();
        message.request.query = uri.getQuery();
        message.request.body.swap(_body);

        void* messagePtr = &message;
        zmq_send(_socket, &messagePtr, sizeof(void*), 0);
        bool done;
        zmq_recv(_socket, &done, sizeof(done), 0);

        Response response;
        try
        {
            response = message.response.get();
        }
        catch (std::future_error& error)
        {
            response.code = http::Code::INTERNAL_SERVER_ERROR;
            ZEROEQINFO << "Error during ConnectionHandler::_handleRequest: "
                       << error.what() << std::endl;
        }

        std::vector<HTTPServer::response_header> headers;
        _addCorsHeaders(headers);

        HTTPServer::response_header contentLength;
        contentLength.name = "Content-Length";
        contentLength.value = std::to_string(response.body.length());
        headers.push_back(contentLength);

        for (auto it = response.headers.begin(); it != response.headers.end();
             ++it)
        {
            HTTPServer::response_header header;
            header.name = _headerEnumToString(it->first);
            header.value = it->second;
            headers.push_back(header);
        }
        const auto status = HTTPServer::connection::status_t(response.code);
        connection->set_status(status);
        connection->set_headers(headers);
        connection->write(response.body);
    }

    void _addCorsHeaders(std::vector<HTTPServer::response_header>& headers)
    {
        // In a typical situation, user agents can discover via a preflight
        // request whether a cross-origin resource is prepared to accept
        // requests. The current implementation of this class does not support
        // this mechanism and has to accept cross-origin resources. In order to
        // achieve that, access control headers are added to all HTTP responses,
        // meaning that all sources are accepted for all requests. More
        // information can be found here: https://www.w3.org/TR/cors

        HTTPServer::response_header allowHeaders;
        allowHeaders.name = "Access-Control-Allow-Headers";
        allowHeaders.value = "Content-Type";

        HTTPServer::response_header allowMethods;
        allowMethods.name = "Access-Control-Allow-Methods";
        allowMethods.value = "GET,POST,PUT,PATCH,DELETE,OPTIONS";

        HTTPServer::response_header allowOrigin;
        allowOrigin.name = "Access-Control-Allow-Origin";
        allowOrigin.value = "*";

        headers.push_back(allowHeaders);
        headers.push_back(allowMethods);
        headers.push_back(allowOrigin);
    }

    const HTTPServer::request& _request;
    void* _socket;
    std::string _body;
    int _size = 0;
};
} // anonymous namespace

RequestHandler::RequestHandler(const std::string& zmqURL, void* zmqContext)
    : _socket(zmq_socket(zmqContext, ZMQ_PAIR))
{
    if (zmq_connect(_socket, zmqURL.c_str()) == -1)
    {
        ZEROEQTHROW(std::runtime_error(
            "Cannot connect RequestHandler to inproc socket"));
    }
}

RequestHandler::~RequestHandler()
{
    zmq_close(_socket);
}

void RequestHandler::operator()(const HTTPServer::request& request,
                                HTTPServer::connection_ptr connection)
{
    // as the underlying cppnetlib http server is asynchronous and payload for
    // PUT events has to be read in chunks in the cppnetlib thread, create
    // a shared instance of the handler object that is passed to cppnetlib for
    // processing the request.
    std::shared_ptr<ConnectionHandler> connectionHandler(
        new ConnectionHandler(request, _socket));
    (*connectionHandler)(connection);
}
}
}
