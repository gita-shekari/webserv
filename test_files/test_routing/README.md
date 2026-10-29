# Routing tests

Build and run from the repository root:

```sh
c++ -Wall -Wextra -Werror -std=c++11 \
    test_files/test_routing/test_routing.cpp \
    -o test_files/test_routing/test_routing

./test_files/test_routing/test_routing
```

The test executable can also be run from `test_files/test_routing`; it detects
the fixture root in either working directory.

The test program is a standalone routing decision harness. Its handler calls
record actions (`CGI`, `STATIC`, `UPLOAD`, and so on) instead of executing CGI,
deleting files, or writing uploads. Keep its routing rules synchronized with
`src/ClientRouting.cpp` while the production handlers are still unfinished.

## Fixtures

Run the test executable from the repository root so this relative root resolves correctly:

```text
test_files/test_routing/fixtures/www
```

Suggested cases:

| Request path | `index` | `autoindex` | Expected branch |
|---|---|---:|---|
| `/listing/` | empty | `true` | list normal, hidden, nested, spaced, and HTML-sensitive names |
| `/listing` | empty | `true` | directory trailing-slash redirect to `/listing/` |
| `/empty/` | empty | `true` | empty listing (only `.` and `..` are returned by `readdir`, then skipped) |
| `/with_index/` | `index.html` | either | regular index file; do not list the directory |
| `/cgi_index/` | `index.py` | either | index file then CGI-extension branch |
| `/no_autoindex/` | empty | `false` | `403 Forbidden` branch |
| `/missing/` | empty | `true` | `404 Not Found` from `stat` |
| `/listing/alpha.txt` | empty | `true` | regular/static file branch |

For the current `test_routing.cpp`, the minimal listing setup is:

```cpp
sc.root = "test_files/test_routing/fixtures/www";
req.path = "/listing/";
lc.root = "";
lc.index = "";
lc.autoindex = true;
size_t indexToTest = 0;
```

Additional cases cover direct CGI files, non-CGI files in a CGI-enabled
location, POST creation through `upload_store`, DELETE routing, configured
redirect priority, method rejection, and a directory whose name ends in
`.py`.
