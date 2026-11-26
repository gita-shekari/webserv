import os
import sys
sys.stdout.buffer.write(b'Content-Type: application/octet-stream\r\n\r\n')
for key in ['REQUEST_METHOD', 'QUERY_STRING', 'CONTENT_LENGTH', 'CONTENT_TYPE', 'SCRIPT_NAME', 'SERVER_PROTOCOL', 'HTTP_COOKIE']:
    sys.stdout.buffer.write((key + '=' + os.environ.get(key, '') + '\n').encode())
assert os.getcwd() == os.path.dirname(os.path.realpath(__file__))
assert 'HTTP_TRANSFER_ENCODING' not in os.environ
assert 'HTTP_PROXY' not in os.environ
sys.stdout.buffer.write(b'x' * 131072)
sys.stdout.buffer.flush()
sys.stdout.buffer.write(sys.stdin.buffer.read())
