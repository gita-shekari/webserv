
#include "Response.hpp"
#include "ResponseBuilder.hpp"
int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	// Server server(8080);
	// server.start();
	Response res;

    res.version = "HTTP/1.1";
    res.statusCode = 200;
    res.reasonPhrase = "OK";
    res.headers["Content-Type"] = "text/plain";
    res.headers["Content-Length"] = "5";
    res.body = "Hello";

    ResponseBuilder builder;

    std::cout << builder.serialize(res) << std::endl;

    Request req;

    req.method = "GET";
    req.path = "/";
    req.version = "HTTP/1.1";
    Response resp = builder.build(req);
    std::cout << builder.serialize(resp) << std::endl;
	return 0;
}
