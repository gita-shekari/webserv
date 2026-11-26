#!/usr/bin/env python3
"""Run after make -B: python3 test_files/test_cgi/test_http.py.
Starts a temporary server and tests CGI through actual HTTP requests.
"""
import concurrent.futures
import http.client
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]


def request(port, path, body=None, chunked=False):
    connection = http.client.HTTPConnection('127.0.0.1', port, timeout=10)
    try:
        connection.request('POST' if body is not None else 'GET', path, body=body,
                           headers={'Content-Type': 'application/octet-stream'},
                           encode_chunked=chunked)
        response = connection.getresponse()
        return response.status, response.read(), response.getheaders()
    finally:
        connection.close()


with tempfile.TemporaryDirectory(prefix='webserv-cgi-http-') as directory:
    work = Path(directory)
    scripts = {
        'echo.py': """import os, sys
sys.stdout.buffer.write(b'Content-Type: application/octet-stream\\r\\n\\r\\n')
sys.stdout.buffer.write((os.environ['REQUEST_METHOD'] + ' ' + os.environ['QUERY_STRING'] + '\\n').encode())
sys.stdout.buffer.write(sys.stdin.buffer.read())
""",
        'status.py': "print('Status: 201 Created\\nContent-Type: text/plain\\nSet-Cookie: a=1\\nSet-Cookie: b=2\\nContent-Length: 999\\n\\ncreated', end='')\n",
        'redirect.py': "print('Location: /echo.py\\n\\n', end='')\n",
        'bad.py': "print('not CGI headers')\n",
        'crash.py': "raise RuntimeError('deliberate test failure')\n",
        'early.py': "import os\nos.close(0)\nprint('Content-Type: text/plain\\n\\nearly', end='')\n",
        'hang.py': "import os, time\nopen('child.pid', 'w').write(str(os.getpid()))\ntime.sleep(30)\n",
        'eofhang.py': "import os, time\nos.close(1)\ntime.sleep(30)\n",
        'large.py': "import sys\nsys.stdout.write('Content-Type: text/plain\\n\\n' + 'x' * 262144)\n",
        'flood.py': "import sys\nsys.stdout.write('Content-Type: text/plain\\n\\n' + 'x' * (11 * 1024 * 1024))\n",
        'index.py': "print('Content-Type: text/plain\\n\\nindex', end='')\n",
    }
    for name, content in scripts.items():
        (work / name).write_text(content)
    (work / 'index.html').write_text('static works')
    (work / 'directory').mkdir()
    (work / 'directory' / 'index.py').write_text(scripts['index.py'])
    with socket.socket() as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    config = work / 'server.conf'
    config.write_text(f'''server {{
    host 127.0.0.1;
    listen {port};
    root {work};
    client_max_body_size 2m;
    location / {{
        methods GET POST;
        index index.py;
        cgi_extension .py;
        cgi_path {sys.executable};
    }}
}}
''')
    with (work / 'server.log').open('w+') as log:
        server = subprocess.Popen([str(ROOT / 'webserv'), str(config)], cwd=ROOT,
                                  stdout=log, stderr=log)
        try:
            for _ in range(100):
                if server.poll() is not None:
                    raise RuntimeError('server exited during startup')
                try:
                    with socket.create_connection(('127.0.0.1', port), timeout=0.1):
                        break
                except OSError:
                    time.sleep(0.05)
            else:
                raise RuntimeError('server did not start')

            assert request(port, '/echo.py?q=a%20b')[:2] == (200, b'GET q=a%20b\n')
            body = bytes(range(256)) * 2048
            assert request(port, '/echo.py', body)[:2] == (200, b'POST \n' + body)
            assert request(port, '/echo.py', iter([b'abc', b'\x00xyz']), True)[:2] == (200, b'POST \nabc\x00xyz')
            assert request(port, '/echo.py', b'')[:2] == (200, b'POST \n')
            assert request(port, '/directory/')[:2] == (200, b'index')
            status, data, headers = request(port, '/status.py')
            assert (status, data) == (201, b'created')
            assert len([v for k, v in headers if k.lower() == 'set-cookie']) == 2
            assert request(port, '/redirect.py')[0] == 302
            for path in ['/bad.py', '/crash.py', '/flood.py']:
                assert request(port, path)[0] == 502, path
            assert request(port, '/early.py', body)[:2] == (200, b'early')
            assert request(port, '/large.py')[:2] == (200, b'x' * 262144)
            print('PASS: HTTP GET, binary/empty/chunked POST, index, status, cookies, redirect, errors, output limit')

            # Preserve jdong's keep-alive and buffered-request handling before CGI.
            connection = http.client.HTTPConnection('127.0.0.1', port, timeout=10)
            try:
                connection.request('GET', '/index.html')
                response = connection.getresponse()
                assert (response.status, response.read()) == (200, b'static works')
                original_socket = connection.sock
                connection.request('GET', '/echo.py?reused=yes')
                response = connection.getresponse()
                assert (response.status, response.read()) == (200, b'GET reused=yes\n')
                assert response.getheader('Connection') == 'close'
                assert original_socket is not None and connection.sock is None
            finally:
                connection.close()

            with socket.create_connection(('127.0.0.1', port), timeout=10) as client:
                client.sendall(b'GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n'
                               b'GET /echo.py?pipelined=yes HTTP/1.1\r\nHost: localhost\r\n\r\n')
                chunks = []
                while True:
                    chunk = client.recv(65536)
                    if not chunk:
                        break
                    chunks.append(chunk)
                first, second = b''.join(chunks).split(b'static works', 1)
                assert first.startswith(b'HTTP/1.1 200 OK\r\n')
                assert second.startswith(b'HTTP/1.1 200 OK\r\n')
                assert second.endswith(b'GET pipelined=yes\n')

            # A client may finish sending and still wait to receive its CGI response.
            with socket.create_connection(('127.0.0.1', port), timeout=10) as client:
                client.sendall(b'POST /echo.py?halfclosed=yes HTTP/1.1\r\nHost: localhost\r\n'
                               b'Content-Length: 3\r\n\r\nabc')
                client.shutdown(socket.SHUT_WR)
                response = http.client.HTTPResponse(client)
                response.begin()
                assert (response.status, response.read()) == (200, b'POST halfclosed=yes\nabc')
            print('PASS: keep-alive to CGI, pipelined static/CGI requests, half-closed POST')

            # A sleeping script must not block unrelated requests, and EOF alone is not completion.
            with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
                started = time.monotonic()
                hanging = pool.submit(request, port, '/hang.py')
                eof_hanging = pool.submit(request, port, '/eofhang.py')
                time.sleep(0.2)
                quick = time.monotonic()
                assert request(port, '/index.html')[:2] == (200, b'static works')
                assert request(port, '/echo.py')[:2] == (200, b'GET \n')
                assert time.monotonic() - quick < 3
                assert hanging.result()[0] == 504
                assert eof_hanging.result()[0] == 504
                assert 5 <= time.monotonic() - started < 9
            pid = int((work / 'child.pid').read_text())
            time.sleep(1.2)
            try:
                os.kill(pid, 0)
            except ProcessLookupError:
                pass
            else:
                raise AssertionError('timed-out child was not reaped')
            print('PASS: five-second timeout, stdout EOF before exit, concurrent requests, child reaping')

            # A reset unambiguously disconnects the client; FIN also permits half-close.
            (work / 'child.pid').unlink()
            client = socket.create_connection(('127.0.0.1', port))
            client.sendall(b'GET /hang.py HTTP/1.1\r\nHost: localhost\r\n\r\n')
            for _ in range(100):
                if (work / 'child.pid').exists():
                    break
                time.sleep(0.02)
            pid = int((work / 'child.pid').read_text())
            client.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
            client.close()
            for _ in range(150):
                try:
                    os.kill(pid, 0)
                except ProcessLookupError:
                    break
                time.sleep(0.02)
            else:
                raise AssertionError('disconnected child was not reaped promptly')
            assert request(port, '/echo.py')[0] == 200
            print('PASS: browser disconnect cleanup; server remains responsive')
        except BaseException:
            log.flush()
            log.seek(0)
            print(log.read(), file=sys.stderr)
            raise
        finally:
            server.terminate()
            try:
                server.wait(timeout=3)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait()
