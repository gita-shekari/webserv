# Request handling component tests

Run from the repository root:

```sh
make test-component-routing
```

The suite uses TestRunner.hpp, the real Client, parser, routing utilities and
response builder. It does not start a listening server or a poll loop.
executeRequest() drives a complete, valid request through production code;
parseResponse() independently inspects its serialized HTTP response.
Intermediate execution failures fail the current case and do not stop later cases.
CGI execution is excluded.

## Coverage

- Static files: text, HTML, nested paths, percent-encoded spaces, custom 404 page.
- Directories: GET trailing-slash redirect, index, index before autoindex,
  missing index, disabled listing, direct files with listing disabled,
  normal and empty listings, nested links, hidden and dot entries, HTML escaping
  and filename URL encoding.
- Methods and redirects: rejected POST/DELETE preserve files and prevent uploads;
  configured redirects, including the current redirect-before-method policy.
- DELETE: success, missing file and directory rejection; check filesystem effects
  and preservation of unrelated files.
- Uploads: POST trailing-slash redirect, successful text and binary uploads,
  missing request target, disabled upload and missing upload store.
- Upload/download round trips: text, binary bytes (including NUL), and a missing
  POST target; follow the returned Location using the same configuration.

Read-only cases use repository fixtures. Each mutating case owns a fresh mkdtemp
folder and prepares copies of the needed fixtures. Its destructor removes created
files and directories even if the case throws. Repository fixtures are never
uploaded into or deleted. Empty directories are created at runtime.
Upload filenames are discovered from the output directory rather than predicted
from the clock or counter. Location checks require an absolute path ending with
the created filename. Dedicated round-trip cases configure /upload with root
and upload_store pointing to the same temporary directory, then GET the returned
Location and compare the downloaded bytes with the uploaded bytes.

## Known failures

The current production buildListingResponse() concatenates filenames directly.
Three comparisons intentionally fail until production code is corrected:

- Spaces in href values must be percent encoded.
- HTML-sensitive displayed filenames must be escaped.
- HTML-sensitive filenames in href values must be percent encoded.

The suite exits nonzero for these failures; they are not skipped or accepted as
expected successes. The tests expect uppercase percent-encoding hex digits.

Error cases configure real custom error pages so fallback-to-500 behavior does
not obscure routing status checks. General parser errors, socket I/O, connection
cleanup, CGI and error-page fallback behavior are outside this suite's coverage.

## CI

The workflow runs unit tests and component tests as separate steps for g++ and
clang++. Matrix fail-fast is disabled so a failure does not cancel the other
compiler. Known listing failures remain ordinary failures and make CI red.
