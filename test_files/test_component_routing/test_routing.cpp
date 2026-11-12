#include "../TestRunner.hpp"
#include "Client.hpp"

#include <algorithm>
#include <cstdlib>
#include <dirent.h>
#include <unistd.h>
#include <vector>
#include <cctype>
#include <cerrno>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>

struct ObservedResponse
{
    int status = 0;
    std::map<std::string, std::string> headers;
    std::string body;
};

static std::string findFixtureRoot()
{
    const char* candidates[] = {
        "test_files/test_component_routing/fixtures/www",
        "fixtures/www"
    };
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
    {
        struct stat info;
        if (stat(candidates[i], &info) == 0 && S_ISDIR(info.st_mode))
            return candidates[i];
    }
    throw std::runtime_error("Cannot find test_component_routing/fixtures/www");
}

static std::string readFile(const std::string& path)
{
    std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Cannot open fixture: " + path);
    std::ostringstream content;
    content << file.rdbuf();
    if (file.bad())
        throw std::runtime_error("Cannot read fixture: " + path);
    return content.str();
}

static ServerConfig makeConfig(const std::string& fixtureRoot)
{
    ServerConfig config;
    config.root = fixtureRoot;
    const int codes[] = {400, 403, 404, 405, 413, 500};
    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i)
        config.error_pages[codes[i]] = "errors/" + std::to_string(codes[i]) + ".html";
    LocationConfig location;
    location.path = "/";
    location.methods.push_back("GET");
    config.locations.push_back(location);
    return config;
}

// Inspect wire output independently of the production ResponseBuilder.
static ObservedResponse parseResponse(const std::string& wire)
{
    const size_t separator = wire.find("\r\n\r\n");
    const size_t firstLineEnd = wire.find("\r\n");
    if (separator == std::string::npos || firstLineEnd > separator)
        throw std::runtime_error("Malformed response: missing header separator");

    ObservedResponse response;
    std::istringstream statusLine(wire.substr(0, firstLineEnd));
    std::string version;
    if (!(statusLine >> version >> response.status) || version != "HTTP/1.1")
        throw std::runtime_error("Malformed response status line");

    size_t cursor = firstLineEnd + 2;
    while (cursor < separator)
    {
        const size_t end = wire.find("\r\n", cursor);
        const std::string line = wire.substr(cursor, end - cursor);
        const size_t colon = line.find(':');
        if (colon == std::string::npos || colon == 0)
            throw std::runtime_error("Malformed response header");
        std::string name = line.substr(0, colon);
        for (size_t i = 0; i < name.size(); ++i)
            name[i] = static_cast<char>(std::tolower(
                static_cast<unsigned char>(name[i])));
        size_t valueStart = colon + 1;
        while (valueStart < line.size()
            && (line[valueStart] == ' ' || line[valueStart] == '\t'))
            ++valueStart;
        response.headers[name] = line.substr(valueStart);
        cursor = end + 2;
    }
    response.body = wire.substr(separator + 4);
    return response;
}

// Drive the real Client. Intermediate states are execution prerequisites,
// rather than separate parser/location assertions in every component case.
static ObservedResponse executeRequest(
    const ServerConfig& config, const std::string& rawRequest)
{
    Client client(-1, 0);
    client.setState(READ_REQ);
    client.setEffectiveMaxBodySize(config.client_max_body_size);
    client.appendReadBuffer(rawRequest.data(), rawRequest.size());

    ParseStatus status = client.parseReadBuffer();
    if (status != NEED_LOCATION)
        throw std::runtime_error("Request execution did not reach location selection");
    client.matchLocation(config.locations);
    const size_t index = client.getLocationIndex();
    if (index >= config.locations.size())
        throw std::runtime_error("Request execution found no matching location");
    const LocationConfig& location = config.locations[index];
    if (location.has_client_max_body_size)
        client.setEffectiveMaxBodySize(location.client_max_body_size);
    status = client.parseReadBuffer();
    if (status != COMPLETE)
        throw std::runtime_error("Request execution could not complete parsing");

    client.setState(REQ_READY);
    client.routing(config);
    if (client.getState() != RESPONSE_READY)
        throw std::runtime_error("Routing did not produce a synchronous response");
    client.prepareResponseForSending();
    return parseResponse(client.getWriteBuffer());
}

static std::string getRequest(const std::string& target)
{
    return "GET " + target + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
}

static void checkFileResponse(TestRunner& tests, const std::string& name,
    const std::string& root, const std::string& target,
    const std::string& fixture, const std::string& contentType)
{
    const std::string expectedBody = readFile(root + fixture);
    const ObservedResponse response = executeRequest(makeConfig(root), getRequest(target));
    tests.checkEqual(name + ": status", 200, response.status);
    tests.checkEqual(name + ": body", expectedBody, response.body);
    const std::map<std::string, std::string>::const_iterator type =
        response.headers.find("content-type");
    tests.checkEqual(name + ": Content-Type", contentType,
        type == response.headers.end() ? std::string("<missing>") : type->second);
    const std::map<std::string, std::string>::const_iterator length =
        response.headers.find("content-length");
    tests.checkEqual(name + ": Content-Length", std::to_string(expectedBody.size()),
        length == response.headers.end() ? std::string("<missing>") : length->second);
}

static void testGetTextFile(TestRunner& tests, const std::string& root)
{
    checkFileResponse(tests, "GET text file", root,
        "/listing/alpha.txt", "/listing/alpha.txt", "text/plain");
}

static void testGetHtmlFile(TestRunner& tests, const std::string& root)
{
    checkFileResponse(tests, "GET HTML file", root,
        "/listing/beta.html", "/listing/beta.html", "text/html");
}

static void testGetNestedFile(TestRunner& tests, const std::string& root)
{
    checkFileResponse(tests, "GET nested file", root,
        "/listing/nested/inside.txt", "/listing/nested/inside.txt", "text/plain");
}

static void testGetFileWithEncodedSpace(TestRunner& tests, const std::string& root)
{
    checkFileResponse(tests, "GET filename with encoded space", root,
        "/listing/space%20name.txt", "/listing/space name.txt", "text/plain");
}

static void testGetMissingFile(TestRunner& tests, const std::string& root)
{
    struct stat info;
    errno = 0;
    if (stat((root + "/missing.txt").c_str(), &info) == 0 || errno != ENOENT)
        throw std::runtime_error("Missing-file fixture requires absent /missing.txt");
    const std::string expectedBody = readFile(root + "/errors/404.html");
    const ObservedResponse response = executeRequest(
        makeConfig(root), getRequest("/missing.txt"));
    tests.checkEqual("GET missing file: status", 404, response.status);
    tests.checkEqual("GET missing file: custom error page", expectedBody, response.body);
    tests.checkEqual("GET missing file: Content-Type", std::string("text/html"),
        response.headers.count("content-type")
            ? response.headers.at("content-type") : std::string("<missing>"));
    tests.checkEqual("GET missing file: Content-Length", std::to_string(expectedBody.size()),
        response.headers.count("content-length")
            ? response.headers.at("content-length") : std::string("<missing>"));
}


// Every mutating case owns a fresh directory. Only paths created by this
// fixture are removed, in reverse order, including during exception unwinding.
class TemporaryFixture
{
public:
    std::string root;

    TemporaryFixture()
    {
        char pattern[] = "/tmp/webserv-routing-XXXXXX";
        char* directory = mkdtemp(pattern);
        if (!directory)
            throw std::runtime_error("Cannot create temporary fixture");
        root = directory;
    }

    ~TemporaryFixture()
    {
        // Uploaded filenames are generated by production code.
        for (std::vector<std::string>::reverse_iterator it = directories.rbegin();
            it != directories.rend(); ++it)
        {
            DIR* directory = opendir(it->c_str());
            if (directory)
            {
                struct dirent* entry;
                while ((entry = readdir(directory)) != NULL)
                {
                    const std::string name(entry->d_name);
                    if (name != "." && name != "..")
                        unlink((*it + "/" + name).c_str());
                }
                closedir(directory);
            }
            rmdir(it->c_str());
        }
        rmdir(root.c_str());
    }

    void directory(const std::string& relative)
    {
        const std::string path = root + relative;
        if (mkdir(path.c_str(), 0700) != 0)
            throw std::runtime_error("Cannot create fixture directory: " + path);
        directories.push_back(path);
    }

    void file(const std::string& relative, const std::string& bytes)
    {
        std::ofstream out((root + relative).c_str(), std::ios::binary);
        out.write(bytes.data(), bytes.size());
        out.close();
        if (!out)
            throw std::runtime_error("Cannot write temporary fixture: " + relative);
    }

    void prepare(const std::string& source)
    {
        directory("/errors");
        const int codes[] = {403, 404, 405, 500};
        for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i)
        {
            const std::string path = "/errors/" + std::to_string(codes[i]) + ".html";
            file(path, readFile(source + path));
        }
        directory("/delete");
        file("/delete/remove-me.txt", readFile(source + "/delete/remove-me.txt"));
        directory("/listing");
        file("/listing/alpha.txt", readFile(source + "/listing/alpha.txt"));
        directory("/uploads");
        directory("/empty");
    }

private:
    std::vector<std::string> directories;
    TemporaryFixture(const TemporaryFixture&);
    TemporaryFixture& operator=(const TemporaryFixture&);
};

static std::vector<std::string> directoryEntries(const std::string& path)
{
    DIR* directory = opendir(path.c_str());
    if (!directory)
        throw std::runtime_error("Cannot inspect directory: " + path);
    std::vector<std::string> names;
    int readError = 0;
    while (true)
    {
        errno = 0;
        struct dirent* entry = readdir(directory);
        if (!entry)
        {
            readError = errno;
            break;
        }
        const std::string name(entry->d_name);
        if (name != "." && name != "..")
            names.push_back(name);
    }
    closedir(directory);
    if (readError)
        throw std::runtime_error("Cannot read directory: " + path);
    std::sort(names.begin(), names.end());
    return names;
}

static bool pathExists(const std::string& path)
{
    struct stat info;
    if (stat(path.c_str(), &info) == 0)
        return true;
    if (errno == ENOENT)
        return false;
    throw std::runtime_error("Cannot inspect path: " + path);
}

static std::string header(const ObservedResponse& response, const std::string& name)
{
    const std::map<std::string, std::string>::const_iterator it = response.headers.find(name);
    return it == response.headers.end() ? "<missing>" : it->second;
}

static void checkLength(TestRunner& tests, const std::string& name,
    const ObservedResponse& response)
{
    tests.checkEqual(name + ": Content-Length", std::to_string(response.body.size()),
        header(response, "content-length"));
}

static void checkError(TestRunner& tests, const std::string& name,
    const ObservedResponse& response, int status, const std::string& root)
{
    tests.checkEqual(name + ": status", status, response.status);
    tests.checkEqual(name + ": error page",
        readFile(root + "/errors/" + std::to_string(status) + ".html"), response.body);
    tests.checkEqual(name + ": Content-Type", std::string("text/html"),
        header(response, "content-type"));
    checkLength(tests, name, response);
}

static std::string request(const std::string& method, const std::string& target,
    const std::string& body = "")
{
    return method + " " + target + " HTTP/1.1\r\nHost: localhost\r\nContent-Length: "
        + std::to_string(body.size()) + "\r\n\r\n" + body;
}

static void checkRedirect(TestRunner& tests, const std::string& name,
    const ObservedResponse& response, int status, const std::string& target)
{
    tests.checkEqual(name + ": status", status, response.status);
    tests.checkEqual(name + ": Location", target, header(response, "location"));
    tests.checkEqual(name + ": empty body", std::string(""), response.body);
    checkLength(tests, name, response);
}

// Directory handling: index selection and autoindex are separate behaviors.
static void testDirectoryRedirect(TestRunner& tests, const std::string& root)
{
    checkRedirect(tests, "GET directory without slash",
        executeRequest(makeConfig(root), getRequest("/listing")), 301, "/listing/");
}

static void testDirectoryIndex(TestRunner& tests, const std::string& root)
{
    ServerConfig config = makeConfig(root);
    config.locations[0].index = "index.html";
    const ObservedResponse response = executeRequest(config, getRequest("/with_index/"));
    tests.checkEqual("directory index: status", 200, response.status);
    tests.checkEqual("directory index: body", readFile(root + "/with_index/index.html"), response.body);
    tests.checkEqual("directory index: Content-Type", std::string("text/html"), header(response, "content-type"));
    checkLength(tests, "directory index", response);
}

static void testIndexBeforeAutoindex(TestRunner& tests, const std::string& root)
{
    ServerConfig config = makeConfig(root);
    config.locations[0].index = "index.html";
    config.locations[0].autoindex = true;
    const ObservedResponse response = executeRequest(config, getRequest("/with_index/"));
    tests.checkEqual("index before autoindex: status", 200, response.status);
    tests.checkEqual("index before autoindex: body", readFile(root + "/with_index/index.html"), response.body);
    tests.checkTrue("index before autoindex: no directory entries",
        response.body.find("should_not_be_listed.txt") == std::string::npos);
    checkLength(tests, "index before autoindex", response);
}

static void testMissingIndex(TestRunner& tests, const std::string& root)
{
    ServerConfig config = makeConfig(root);
    config.locations[0].index = "missing.html";
    config.locations[0].autoindex = true;
    checkError(tests, "missing index does not fall back to listing",
        executeRequest(config, getRequest("/with_index/")), 404, root);
}

static void testListingForbidden(TestRunner& tests, const std::string& root)
{
    checkError(tests, "autoindex disabled",
        executeRequest(makeConfig(root), getRequest("/no_autoindex/")), 403, root);
}

static void testFileWithoutAutoindex(TestRunner& tests, const std::string& root)
{
    checkFileResponse(tests, "direct file with autoindex disabled", root,
        "/no_autoindex/file.txt", "/no_autoindex/file.txt", "text/plain");
}

static void testDirectoryListing(TestRunner& tests, const std::string& root)
{
    ServerConfig config = makeConfig(root);
    config.locations[0].autoindex = true;
    const ObservedResponse response = executeRequest(config, getRequest("/listing/"));
    tests.checkEqual("directory listing: status", 200, response.status);
    tests.checkEqual("directory listing: Content-Type", std::string("text/html"), header(response, "content-type"));
    checkLength(tests, "directory listing", response);
    const char* links[] = {
        "href=\"/listing/alpha.txt\">alpha.txt</a>",
        "href=\"/listing/beta.html\">beta.html</a>",
        "href=\"/listing/nested/\">nested/</a>"
    };
    for (size_t i = 0; i < sizeof(links) / sizeof(links[0]); ++i)
        tests.checkTrue(std::string("directory listing: ") + links[i], response.body.find(links[i]) != std::string::npos);
    tests.checkTrue("directory listing: hides hidden files", response.body.find(".hidden.txt") == std::string::npos);
    tests.checkTrue("directory listing: excludes dot entry", response.body.find("href=\"/listing/.\"") == std::string::npos);
    tests.checkTrue("directory listing: excludes parent entry", response.body.find("href=\"/listing/..\"") == std::string::npos);
    tests.checkTrue("directory listing: encodes spaces in links",
        response.body.find("href=\"/listing/space%20name.txt\">space name.txt</a>") != std::string::npos);
    tests.checkTrue("directory listing: escapes HTML-sensitive display text",
        response.body.find("&lt;tag&gt;&amp;.txt</a>") != std::string::npos);
    tests.checkTrue("directory listing: encodes HTML-sensitive filename in link",
        response.body.find("href=\"/listing/%3Ctag%3E%26.txt\"") != std::string::npos);
}

static void testEmptyListing(TestRunner& tests, const std::string& source)
{
    TemporaryFixture fixture;
    fixture.prepare(source);
    ServerConfig config = makeConfig(fixture.root);
    config.locations[0].autoindex = true;
    const ObservedResponse response = executeRequest(config, getRequest("/empty/"));
    tests.checkEqual("empty listing: status", 200, response.status);
    tests.checkTrue("empty listing: contains list", response.body.find("<ul>") != std::string::npos);
    tests.checkTrue("empty listing: no entries", response.body.find("<li>") == std::string::npos);
    checkLength(tests, "empty listing", response);
}

// Method rejection and redirects must not cause filesystem mutations.
static void testPostNotAllowed(TestRunner& tests, const std::string& source)
{
    TemporaryFixture fixture;
    fixture.prepare(source);
    ServerConfig config = makeConfig(fixture.root);
    config.locations[0].upload_enabled = true;
    config.locations[0].upload_store = fixture.root + "/uploads";
    checkError(tests, "POST not allowed",
        executeRequest(config, request("POST", "/listing/alpha.txt", "replacement")), 405, fixture.root);
    tests.checkEqual("POST not allowed: original unchanged", readFile(source + "/listing/alpha.txt"),
        readFile(fixture.root + "/listing/alpha.txt"));
    tests.checkTrue("POST not allowed: no upload", directoryEntries(fixture.root + "/uploads").empty());
}

static void testDeleteNotAllowed(TestRunner& tests, const std::string& source)
{
    TemporaryFixture fixture;
    fixture.prepare(source);
    checkError(tests, "DELETE not allowed", executeRequest(makeConfig(fixture.root),
        request("DELETE", "/delete/remove-me.txt")), 405, fixture.root);
    tests.checkEqual("DELETE not allowed: original unchanged", readFile(source + "/delete/remove-me.txt"),
        readFile(fixture.root + "/delete/remove-me.txt"));
}

static void testConfiguredRedirect(TestRunner& tests, const std::string& root)
{
    ServerConfig config = makeConfig(root);
    config.locations[0].has_redirect = true;
    config.locations[0].redirect_code = 302;
    config.locations[0].redirect_target = "/new-place";
    checkRedirect(tests, "configured redirect", executeRequest(config, getRequest("/missing.txt")), 302, "/new-place");
}

static void testRedirectBeforeMethodCheck(TestRunner& tests, const std::string& source)
{
    TemporaryFixture fixture;
    fixture.prepare(source);
    ServerConfig config = makeConfig(fixture.root);
    config.locations[0].has_redirect = true;
    config.locations[0].redirect_code = 301;
    config.locations[0].redirect_target = "/new-place";
    checkRedirect(tests, "redirect before method rejection", executeRequest(config,
        request("DELETE", "/delete/remove-me.txt")), 301, "/new-place");
    tests.checkEqual("redirect: original unchanged", readFile(source + "/delete/remove-me.txt"),
        readFile(fixture.root + "/delete/remove-me.txt"));
}

static void testDeleteFile(TestRunner& tests, const std::string& source)
{
    TemporaryFixture fixture;
    fixture.prepare(source);
    ServerConfig config = makeConfig(fixture.root);
    config.locations[0].methods.push_back("DELETE");
    const ObservedResponse response = executeRequest(config, request("DELETE", "/delete/remove-me.txt"));
    tests.checkEqual("DELETE file: status", 204, response.status);
    tests.checkEqual("DELETE file: empty body", std::string(""), response.body);
    tests.checkTrue("DELETE file: target removed", !pathExists(fixture.root + "/delete/remove-me.txt"));
    tests.checkEqual("DELETE file: unrelated file unchanged", readFile(source + "/listing/alpha.txt"),
        readFile(fixture.root + "/listing/alpha.txt"));
}

static void testDeleteMissingFile(TestRunner& tests, const std::string& source)
{
    TemporaryFixture fixture;
    fixture.prepare(source);
    ServerConfig config = makeConfig(fixture.root);
    config.locations[0].methods.push_back("DELETE");
    checkError(tests, "DELETE missing file", executeRequest(config,
        request("DELETE", "/delete/missing.txt")), 404, fixture.root);
    tests.checkTrue("DELETE missing file: path remains absent", !pathExists(fixture.root + "/delete/missing.txt"));
    tests.checkEqual("DELETE missing file: existing file unchanged", readFile(source + "/delete/remove-me.txt"),
        readFile(fixture.root + "/delete/remove-me.txt"));
}

static void testDeleteDirectory(TestRunner& tests, const std::string& source)
{
    TemporaryFixture fixture;
    fixture.prepare(source);
    ServerConfig config = makeConfig(fixture.root);
    config.locations[0].methods.push_back("DELETE");
    checkError(tests, "DELETE directory", executeRequest(config,
        request("DELETE", "/listing/")), 403, fixture.root);
    tests.checkTrue("DELETE directory: directory preserved", pathExists(fixture.root + "/listing"));
    tests.checkEqual("DELETE directory: contents preserved", readFile(source + "/listing/alpha.txt"),
        readFile(fixture.root + "/listing/alpha.txt"));
}

static ServerConfig uploadConfig(const TemporaryFixture& fixture)
{
    ServerConfig config = makeConfig(fixture.root);
    config.locations[0].methods.push_back("POST");
    config.locations[0].upload_enabled = true;
    config.locations[0].upload_store = fixture.root + "/uploads";
    return config;
}

static void testPostDirectoryRedirect(TestRunner& tests, const std::string& source)
{
    TemporaryFixture fixture;
    fixture.prepare(source);
    checkRedirect(tests, "POST directory without slash", executeRequest(uploadConfig(fixture),
        request("POST", "/listing", "payload")), 307, "/listing/");
    tests.checkTrue("POST redirect: no upload", directoryEntries(fixture.root + "/uploads").empty());
}

static void checkUpload(TestRunner& tests, const std::string& source,
    const std::string& name, const std::string& target, const std::string& body)
{
    TemporaryFixture fixture;
    fixture.prepare(source);
    const ObservedResponse response = executeRequest(uploadConfig(fixture), request("POST", target, body));
    tests.checkEqual(name + ": status", 201, response.status);
    tests.checkEqual(name + ": empty response body", std::string(""), response.body);
    checkLength(tests, name, response);
    const std::vector<std::string> files = directoryEntries(fixture.root + "/uploads");
    tests.checkEqual(name + ": one new file", static_cast<size_t>(1), files.size());
    if (files.size() != 1)
        return;
    tests.checkEqual(name + ": uploaded bytes", body, readFile(fixture.root + "/uploads/" + files[0]));
    const std::string location = header(response, "location");
    tests.checkTrue(name + ": Location is an absolute URL path", !location.empty() && location[0] == '/');
    tests.checkTrue(name + ": Location ends with created filename",
        location.size() >= files[0].size() && location.compare(location.size() - files[0].size(), files[0].size(), files[0]) == 0);
    tests.checkEqual(name + ": unrelated file unchanged", readFile(source + "/listing/alpha.txt"),
        readFile(fixture.root + "/listing/alpha.txt"));
}

static void testUpload(TestRunner& tests, const std::string& source)
{
    checkUpload(tests, source, "POST upload", "/listing/", "uploaded content\n");
}

static void testBinaryUpload(TestRunner& tests, const std::string& source)
{
    checkUpload(tests, source, "POST binary upload", "/listing/", std::string("a\0b\r\n", 5));
}

static void testUploadMissingTarget(TestRunner& tests, const std::string& source)
{
    checkUpload(tests, source, "POST missing target", "/new-resource.txt", "new resource");
}

// GET root and POST storage share a directory, as in the real /upload config.
// Follow Location verbatim: do not construct the URL used for the GET request.
static void checkUploadDownload(TestRunner& tests, const std::string& source,
    const std::string& name, const std::string& target, const std::string& body)
{
    TemporaryFixture fixture;
    fixture.prepare(source);
    ServerConfig config = uploadConfig(fixture);
    config.locations[0].path = "/upload";
    config.locations[0].root = fixture.root + "/uploads";

    const ObservedResponse uploaded = executeRequest(config, request("POST", target, body));
    tests.checkEqual(name + ": POST status", 201, uploaded.status);
    const std::vector<std::string> files = directoryEntries(fixture.root + "/uploads");
    tests.checkEqual(name + ": one saved file", static_cast<size_t>(1), files.size());
    if (files.size() != 1)
        return;
    const std::string location = header(uploaded, "location");
    tests.checkEqual(name + ": Location maps to saved file",
        std::string("/upload/") + files[0], location);
    if (location == "<missing>" || location.empty() || location[0] != '/')
        return;

    const ObservedResponse downloaded = executeRequest(config, getRequest(location));
    tests.checkEqual(name + ": GET status", 200, downloaded.status);
    tests.checkEqual(name + ": downloaded bytes equal uploaded bytes", body, downloaded.body);
    checkLength(tests, name + " download", downloaded);
    tests.checkEqual(name + ": saved bytes", body,
        readFile(fixture.root + "/uploads/" + files[0]));
}

static void testUploadDownload(TestRunner& tests, const std::string& source)
{
    checkUploadDownload(tests, source, "upload/download text", "/upload/", "uploaded content\n");
}

static void testBinaryUploadDownload(TestRunner& tests, const std::string& source)
{
    checkUploadDownload(tests, source, "upload/download binary",
        "/upload/", std::string("a\0b\r\n", 5));
}

static void testMissingTargetUploadDownload(TestRunner& tests, const std::string& source)
{
    checkUploadDownload(tests, source, "upload/download missing target",
        "/upload/new-resource.txt", "new resource");
}

static void testUploadDisabled(TestRunner& tests, const std::string& source)
{
    TemporaryFixture fixture;
    fixture.prepare(source);
    ServerConfig config = uploadConfig(fixture);
    config.locations[0].upload_enabled = false;
    checkError(tests, "upload disabled", executeRequest(config,
        request("POST", "/listing/alpha.txt", "payload")), 403, fixture.root);
    tests.checkTrue("upload disabled: no new file", directoryEntries(fixture.root + "/uploads").empty());
    tests.checkEqual("upload disabled: existing file unchanged", readFile(source + "/listing/alpha.txt"),
        readFile(fixture.root + "/listing/alpha.txt"));
}

static void testUploadMissingStore(TestRunner& tests, const std::string& source)
{
    TemporaryFixture fixture;
    fixture.prepare(source);
    ServerConfig config = uploadConfig(fixture);
    config.locations[0].upload_store = fixture.root + "/missing-store";
    checkError(tests, "missing upload store", executeRequest(config,
        request("POST", "/listing/", "payload")), 500, fixture.root);
    tests.checkTrue("missing upload store: not created", !pathExists(config.locations[0].upload_store));
    tests.checkTrue("missing upload store: no files elsewhere", directoryEntries(fixture.root + "/uploads").empty());
}

typedef void (*TestCase)(TestRunner&, const std::string&);

static void runCase(TestRunner& tests, const std::string& name,
    TestCase test, const std::string& root)
{
    try
    {
        test(tests, root);
    }
    catch (const std::exception& error)
    {
        tests.checkTrue(name + ": setup/execution failed: " + error.what(), false);
    }
}

int main()
{
    TestRunner tests;
    try
    {
        const std::string root = findFixtureRoot();
        runCase(tests, "GET text file", testGetTextFile, root);
        runCase(tests, "GET HTML file", testGetHtmlFile, root);
        runCase(tests, "GET nested file", testGetNestedFile, root);
        runCase(tests, "GET filename with encoded space", testGetFileWithEncodedSpace, root);
        runCase(tests, "GET missing file", testGetMissingFile, root);
        runCase(tests, "testDirectoryRedirect", testDirectoryRedirect, root);
        runCase(tests, "testDirectoryIndex", testDirectoryIndex, root);
        runCase(tests, "testIndexBeforeAutoindex", testIndexBeforeAutoindex, root);
        runCase(tests, "testMissingIndex", testMissingIndex, root);
        runCase(tests, "testListingForbidden", testListingForbidden, root);
        runCase(tests, "testFileWithoutAutoindex", testFileWithoutAutoindex, root);
        runCase(tests, "testDirectoryListing", testDirectoryListing, root);
        runCase(tests, "testEmptyListing", testEmptyListing, root);
        runCase(tests, "testPostNotAllowed", testPostNotAllowed, root);
        runCase(tests, "testDeleteNotAllowed", testDeleteNotAllowed, root);
        runCase(tests, "testConfiguredRedirect", testConfiguredRedirect, root);
        runCase(tests, "testRedirectBeforeMethodCheck", testRedirectBeforeMethodCheck, root);
        runCase(tests, "testDeleteFile", testDeleteFile, root);
        runCase(tests, "testDeleteMissingFile", testDeleteMissingFile, root);
        runCase(tests, "testDeleteDirectory", testDeleteDirectory, root);
        runCase(tests, "testPostDirectoryRedirect", testPostDirectoryRedirect, root);
        runCase(tests, "testUpload", testUpload, root);
        runCase(tests, "testBinaryUpload", testBinaryUpload, root);
        runCase(tests, "testUploadMissingTarget", testUploadMissingTarget, root);
        runCase(tests, "testUploadDownload", testUploadDownload, root);
        runCase(tests, "testBinaryUploadDownload", testBinaryUploadDownload, root);
        runCase(tests, "testMissingTargetUploadDownload", testMissingTargetUploadDownload, root);
        runCase(tests, "testUploadDisabled", testUploadDisabled, root);
        runCase(tests, "testUploadMissingStore", testUploadMissingStore, root);
    }
    catch (const std::exception& error)
    {
        tests.checkTrue(std::string("Fixture setup failed: ") + error.what(), false);
    }
    return tests.finish();
}
