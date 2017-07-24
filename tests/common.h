
/* Copyright (c) 2014-2017, Human Brain Project
 *                          Daniel Nachbaur <daniel.nachbaur@epfl.ch>
 *                          Stefan.Eilemann@epfl.ch
 *                          Juan Hernando <jhernando@fi.upm.es>
 */

#include <zeroeq/detail/port.h>
#include <zeroeq/zeroeq.h>

#include <boost/test/unit_test.hpp>
#include <servus/serializable.h>
#include <servus/uri.h>
#include <string>

#ifdef _WIN32
#include <process.h>
#define getpid _getpid
#else
#include <sys/types.h>
#include <unistd.h>
#endif

namespace test
{
std::string buildUniqueSession()
{
    std::string name =
        std::string(boost::unit_test::framework::current_test_case().p_name);
    std::replace(name.begin(), name.end(), '_', '-');
    return name + std::to_string(getpid());
}

zeroeq::URI buildURI(const std::string& hostname, const zeroeq::Publisher& to)
{
    zeroeq::URI uri = to.getURI();
    uri.setHost(hostname);
    assert(uri.getPort());
    return uri;
}

const std::string echoMessage("So long, and thanks for all the fish!");

class Echo : public servus::Serializable
{
public:
    static std::string TYPENAME() { return "zeroeq::test::Echo"; }
    static servus::uint128_t IDENTIFIER()
    {
        return servus::make_uint128(TYPENAME());
    }

    std::string getTypeName() const final { return TYPENAME(); }
    Echo() {}
    explicit Echo(const std::string& message)
        : _message(message)
    {
    }
    const std::string& getMessage() const { return _message; }
    bool operator==(const Echo& rhs) const { return _message == rhs._message; }
private:
    bool _fromBinary(const void* data, const size_t size) final
    {
        _message = std::string(static_cast<const char*>(data), size);
        return true;
    }

    Data _toBinary() const final
    {
        Data data;
        data.ptr =
            std::shared_ptr<const void>(_message.data(), [](const void*) {});
        data.size = _message.length();
        return data;
    }

    std::string _message;
};

class Empty : public servus::Serializable
{
public:
    static std::string TYPENAME() { return "zeroeq::test::Empty"; }
    static servus::uint128_t IDENTIFIER()
    {
        return servus::make_uint128(TYPENAME());
    }

    std::string getTypeName() const final { return TYPENAME(); }
private:
    bool _fromBinary(const void*, const size_t) final { return true; }
    Data _toBinary() const final { return Data(); }
};

void onEchoEvent(const void* data, const size_t size)
{
    const std::string message(reinterpret_cast<const char*>(data), size);
    BOOST_CHECK_EQUAL(size, message.size());
    BOOST_CHECK_EQUAL(echoMessage, message);
}

class Monitor : public zeroeq::Monitor
{
public:
    explicit Monitor(zeroeq::Sender& sender)
        : zeroeq::Monitor(sender)
    {
    }

    Monitor(zeroeq::Sender& sender, zeroeq::Receiver& shared)
        : zeroeq::Monitor(sender, shared)
    {
    }

    void notifyNewConnection() final { ++connections; }
    size_t connections{0};
};
}
