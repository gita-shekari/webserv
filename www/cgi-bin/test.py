import os
import sys


body = sys.stdin.buffer.read()
print("Content-Type: text/plain; charset=utf-8")
print()
print("CGI is working!")
print("Method: " + os.environ.get("REQUEST_METHOD", ""))
print("Query string: " + os.environ.get("QUERY_STRING", ""))
print("Content type: " + os.environ.get("CONTENT_TYPE", ""))
print("Body length: " + str(len(body)))
if body:
    print("Body: " + body.decode("utf-8", errors="replace"))
