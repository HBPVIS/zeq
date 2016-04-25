Publishing the REST API exposed by the HTTP Server
============

The http server exposes C++ objects via a REST API, in a JSON serialized format.
The purpose of this document is to describe how this API request is implemented,
and invoked.

## Requirements

* Define how to invoke the application REST API
* Implement the JSON representation of the REST API

## API

GET /registry returns the URIs for all registered objects, together with the
supported REST operation (GET or PUT). Each objet can be queried for its schema
using a GET operation on the URI of that object, suffixed by /schema.

- Example:
  - Get all URIs of registered objects:
    * Request: ``` GET http://hostname:port/registry ```
    * Response:
```
    {
      { "namespace/foo": ["GET", "PUT"] },
      { "namespace/bar": ["PUT"] }
    }
```
  - Get JSON schema for a specific object:
    * Request: ```GET http://hostname:port/namespace/foo/schema```
    * Response:```{ "foo": { "name": string, "age": integer } }```

## Examples
Create an HTTP server and register serializables:
```
  void setupHTTPServer( const int argc, char** argv )
  {
    // Create HTTP server
    _httpServer = zeq::http::Server::parse( argc, argv );
    if( !_httpServer )
      return;

    // Register C++ objects
    subscribers.push_back( _httpServer );
    _httpServer.subscribe( camera, LUT, vrParams );
    _httpServer.register( camera, LUT, vrParams, frame );
  }
```
Query API:
   * Request: ```GET http://hostname:port/zerobuf/render/camera/schema```
   * Response: ```{ "camera" : { "origin": "string", "lookat": "up" } }```

## Implementation
- Add a getSchema public method to Servus::serializable
- Make sure getSchema is implemented by to the code generator and returns the
  content of the .fbs file.
- _registry_ and _schema_ are both keywords that correspond to GET requests.
  They both need to be handled by the _processGet method of the
  zeq::http::Server implementation.
- Add a getRegistry method to the zeq::http::Server implementation that returns
  the contents of _registrations and _subscriptions containers in the form of
  a JSON list a tuples defined by a URI and a list of REST operations (currently
  GET and PUT).
- This mechanism should be transparent to the user of the class, and should be
  handled internally to the zeq::http::Server. The schema can be requested at
  any time on invocation of the getSchema method on the registered object.

## Issues
### API
Three options are being discussed regarding the implementation of the API:

#### Option 1: per-object invocation
GET / returns the list of top-level namespaces, e.g., 'zerobuf', GET
/zerobuf/render/ returns the objects in zerobuf::render. GET
/namespace/object/schema returns the schema for the object.

- Example:
  * Request: ```GET http://hostname:port/```
  * Response: ```[ "namespace1", "namespace2", "..." ]```
  * Request: ```GET http://hostname:port/namespace1/```
  * Response: ```[ "object1", "object2", "..." ]```
  * Request: ```GET http://hostname:port/namespace1/object1/schema```
  * Response: ```{ "object1" : { "attribute1": "string", "attribute2": "boolean" } }```
- Pros: Clean API, and a one-to-one match between a URI and the registered
  object namespace and name.
- Cons: Client needs to traverse the tree of URIs to get the list of registered
  objects. A mechanism to distinguish between an object and a namespace is
  needed.

#### Option 2: Unique call for all objects
GET /api-docs returns the list of URIs and schemas for registered objects.
- Example:
  * Request: ```GET http://hostname:port/api-docs```
  * Response:
```
{
  { "namespace/foo": { foo: { "name": string } },
  { "namespace/bar": { bar: { "x": float, "y": float, "z": float } }
}
```
- Pros: Simple client and server side implementations. No need for a recursive
  walk in the tree of URIs. Only one invocation is required to get the list of
  all registered objects.
- Cons: /api-docs becomes a reserved URI.

#### Option 3: Mix of global registry and per-objet requests
This option is a mix of option 1 and 2. GET /registry returns the URIs for all
registered objects, together with the supported REST operation (GET or PUT).
Each objet can be queried for its schema using the a GET operation on the URI of
that object, suffixed by /schema.

- Example:
  - Get URIs to registered objects:
    * Request: ```GET http://hostname:port/registry```
    * Response:
```
{
  { "namespace/foo": ["GET", "PUT"] },
  { "namespace/bar": ["PUT"] }
}
```
  - Get JSON schema for a specific object:
    * Request: ```GET http://hostname:port/namespace/foo/schema```
    * Response: ```{ "schema": { "name": string, "age": integer } }```
- Pros: Clean API, simple client and server side implementations. No need for a
  recursive walk in the tree of URIs. Specific object schemas are requested on
  demand.
- cons: 'registry' and 'schema' become keywords and cannot be used by the
  application to expose objects with those names.
