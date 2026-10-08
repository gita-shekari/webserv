# Routing test fixtures

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

The current test main creates one request and one location, so the only valid index is `0`.
