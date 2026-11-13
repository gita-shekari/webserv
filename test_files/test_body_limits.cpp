#include "Server.hpp"
#include "ConfigParser.hpp"
#include "TestRunner.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// Keep production Client creation/destruction messages out of the test report.
class QuietClientOutput
{
    std::ostringstream _capture;
    std::streambuf* _original;
public:
    QuietClientOutput() : _original(std::cout.rdbuf(_capture.rdbuf())) {}
    ~QuietClientOutput() { std::cout.rdbuf(_original); }
private:
    QuietClientOutput(const QuietClientOutput&);
    QuietClientOutput& operator=(const QuietClientOutput&);
};

struct LimitCase
{
    const char* name;
    const char* path;
    size_t size;
    const char* location;
    size_t limit;
    bool rejected;
};

static std::string statusName(ParseStatus status)
{
    switch (status)
    {
        case INCOMPLETE: return "INCOMPLETE";
        case NEED_LOCATION: return "NEED_LOCATION";
        case COMPLETE: return "COMPLETE";
        case ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

static std::string headers(const std::string& path, size_t size, bool chunked)
{
    std::ostringstream out;
    out << "POST " << path << " HTTP/1.1\r\nHost: localhost\r\n";
    if (chunked)
        out << "Transfer-Encoding: chunked\r\n";
    else
        out << "Content-Length: " << size << "\r\n";
    out << "\r\n";
    return out.str();
}

static std::string encodeBody(const std::string& body, bool chunked)
{
    if (!chunked)
        return body;
    // Two individually small chunks also exercise the cumulative limit.
    const size_t cut = body.size() / 2;
    std::ostringstream out;
    if (cut != 0)
        out << std::hex << cut << "\r\n" << body.substr(0, cut) << "\r\n";
    if (cut != body.size())
        out << std::hex << body.size() - cut << "\r\n"
            << body.substr(cut) << "\r\n";
    out << "0\r\n\r\n";
    return out.str();
}

static void checkLocation(TestRunner& tests, const std::string& name,
    const ServerConfig& config, const Client& client, const char* expected)
{
    const size_t index = client.getLocationIndex();
    tests.checkTrue(name + ": matching location exists", index < config.locations.size());
    if (index < config.locations.size())
        tests.checkEqual(name + ": selected location", std::string(expected),
            config.locations[index].path);
}

static void reportCase(const std::string& name, int failures)
{
    if (failures == 0)
        std::cout << "[PASS] " << name << '\n';
    else
        std::cerr << "[FAIL] " << name << ": " << failures << " assertion(s) failed\n";
}

static void runCase(TestRunner& tests, const std::vector<ServerConfig>& configs,
    const LimitCase& item, bool chunked)
{
    const std::string name = std::string(chunked ? "Chunked: " : "Content-Length: ")
        + item.name;
    const int before = tests.failedCount();
    {
        QuietClientOutput quiet;
        Server server(configs); // Construct only; start() is deliberately not called.
        Client client(-1, 0); // No socket is opened or read by this test.
        const std::string body(item.size, 'x');
        const std::string request = headers(item.path, body.size(), chunked)
            + encodeBody(body, chunked);
        client.appendReadBuffer(request.data(), request.size());

        // Invoke exactly the handoff used by Server::receiveClientData().
        const ParseStatus status = server.parseClientRequest(client);
        checkLocation(tests, name, configs[0], client, item.location);
        tests.checkEqual(name + ": effective limit", item.limit,
            client.getEffectiveMaxBodySize());
        tests.checkEqual(name + ": parse status",
            statusName(item.rejected ? ERROR : COMPLETE), statusName(status));
        const Request parsed = client.getReq();
        tests.checkEqual(name + ": HTTP status",
            item.rejected ? PAYLOAD_TOO_LARGE : REQ_OK, parsed.httpStatus);
        if (!item.rejected)
            tests.checkEqual(name + ": complete body", body, parsed.body);
    }
    reportCase(name, tests.failedCount() - before);
}

static void testFragmentedBody(TestRunner& tests, const std::vector<ServerConfig>& configs, bool chunked)
{
    const std::string name = std::string(chunked ? "Chunked: " : "Content-Length: ")
        + "retains the selected limit while waiting for body fragments";
    const int before = tests.failedCount();
    {
        QuietClientOutput quiet;
        Server server(configs); // Construct only; start() is deliberately not called.
        Client client(-1, 0);
        const std::string first = headers("/strict/file", 4, chunked);
        client.appendReadBuffer(first.data(), first.size());
        ParseStatus status = server.parseClientRequest(client);
        tests.checkEqual(name + ": waits for body", statusName(INCOMPLETE), statusName(status));
        tests.checkEqual(name + ": initial limit", size_t(4), client.getEffectiveMaxBodySize());
        const std::string rest = encodeBody("abcd", chunked);
        client.appendReadBuffer(rest.data(), rest.size());
        status = server.parseClientRequest(client);
        tests.checkEqual(name + ": completes", statusName(COMPLETE), statusName(status));
        tests.checkEqual(name + ": limit is not reset to server default",
            size_t(4), client.getEffectiveMaxBodySize());
        tests.checkEqual(name + ": body", std::string("abcd"), client.getReq().body);
    }
    reportCase(name, tests.failedCount() - before);
}

static void testEarlyRejection(TestRunner& tests, const std::vector<ServerConfig>& configs)
{
    const std::string name = "Content-Length: rejects oversized declaration before body arrives";
    const int before = tests.failedCount();
    {
        QuietClientOutput quiet;
        Server server(configs); // Construct only; start() is deliberately not called.
        Client client(-1, 0);
        const std::string request = headers("/strict/file", 5, false);
        client.appendReadBuffer(request.data(), request.size());
        const ParseStatus status = server.parseClientRequest(client);
        tests.checkEqual(name + ": uses location limit", size_t(4), client.getEffectiveMaxBodySize());
        tests.checkEqual(name + ": status", statusName(ERROR), statusName(status));
        tests.checkEqual(name + ": HTTP status", PAYLOAD_TOO_LARGE, client.getReq().httpStatus);
    }
    reportCase(name, tests.failedCount() - before);
}

int main(int argc, char** argv)
{
    Logger::setLevel(Logger::WARNING);
    TestRunner tests;
    tests.setPrintPass(false);
    try
    {
        const std::string fixture = argc > 1 ? argv[1] : "test_files/fixtures/body_limits.conf";
        ConfigParser parser;
        const std::vector<ServerConfig> configs = parser.parseConfig(fixture);
        if (configs.size() != 1)
            throw std::runtime_error("fixture must contain one server");
        const LimitCase cases[] = {
            {"inherits server limit and rejects oversize body", "/inherit/file", 11, "/inherit", 10, true},
            {"inherits server limit and accepts smaller body", "/inherit/file", 9, "/inherit", 10, false},
            {"accepts body exactly at inherited limit", "/inherit/file", 10, "/inherit", 10, false},
            {"uses stricter location limit", "/strict/file", 5, "/strict", 4, true},
            {"accepts body exactly at location limit", "/strict/file", 4, "/strict", 4, false},
            {"allows location limit above server default", "/loose/file", 11, "/loose", 20, false},
            {"rejects body exceeding the larger location limit", "/loose/file", 21, "/loose", 20, true},
            {"uses the most specific location", "/strict/private/file", 3, "/strict/private", 2, true},
            {"does not confuse a path prefix with a location boundary", "/strictly/file", 5, "/", 10, false}
        };
        for (int chunked = 0; chunked < 2; ++chunked)
        {
            for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
                runCase(tests, configs, cases[i], chunked != 0);
            testFragmentedBody(tests, configs, chunked != 0);
        }
        testEarlyRejection(tests, configs);
    }
    catch (const std::exception& error)
    {
        std::cerr << "[ERROR] body limit test setup: " << error.what() << '\n';
        tests.checkTrue("test setup succeeds", false);
    }
    return tests.finish();
}
