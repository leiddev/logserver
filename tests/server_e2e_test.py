#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
#
# Leid 2026.11
#
"""
End-to-end test for the LogServer executable.

Usage: server_e2e_test.py <path-to-LogServer> [webserver-type]

Checks that the UDP -> TCP fan-out, the web front end and the shutdown path
work on the current platform. Log file archiving is covered by the unit
tests in app_log_test.cc; here we only check that a log file appears exactly
when the log directory is on a temporary file system.
"""

import base64
import os
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time

TIMEOUT = 10.0

# "测试中文日志" in both encodings. Log producers here usually send UTF-8, but
# GB2312 and plain garbage reach the server too, so both paths are exercised.
GB2312_CHINESE = b"\xb2\xe2\xca\xd4\xd6\xd0\xce\xc4\xc8\xd5\xd6\xbe"
GB2312_CHINESE_UTF8 = b"\xe6\xb5\x8b\xe8\xaf\x95\xe4\xb8\xad\xe6\x96\x87\xe6\x97\xa5\xe5\xbf\x97"
INVALID_UTF8 = b"\xe6\xb5\x8b\xe8"  # neither valid UTF-8 nor valid GB2312
REPLACEMENT = b"\xef\xbf\xbd"        # U+FFFD


def free_port(sock_type):
    """Ask the OS for an unused port of the given socket type."""
    sock = socket.socket(socket.AF_INET, sock_type)
    sock.bind(("127.0.0.1", 0))
    port = sock.getsockname()[1]
    sock.close()
    return port


def wait_for_tcp(port, deadline):
    while time.time() < deadline:
        try:
            conn = socket.create_connection(("127.0.0.1", port), timeout=1)
            conn.settimeout(3)
            return conn
        except OSError:
            time.sleep(0.1)
    return None


def recv_until(sock, expected, timeout):
    sock.settimeout(timeout)
    data = b""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            chunk = sock.recv(4096)
        except socket.timeout:
            break
        if not chunk:
            break
        data += chunk
        if expected in data:
            break
    return data


def is_tmpfs(path):
    if sys.platform == "win32":
        # Windows has no tmpfs; isTempFS() returns true there by design.
        return True
    try:
        output = subprocess.check_output(["stat", "-f", "-c", "%T", path], text=True)
    except Exception:
        return False
    return output.strip() == "tmpfs"


def fetch_page(port):
    """GET / and return the raw response bytes."""
    http = socket.create_connection(("127.0.0.1", port), timeout=5)
    http.sendall(b"GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n")
    page = recv_until(http, b"</html>", 5)
    http.close()
    return page


def websocket_handshake(port):
    """Send a WebSocket upgrade request and return the status line."""
    conn = websocket_connect(port)
    if conn is None:
        return b""
    conn.close()
    return b"HTTP/1.1 101 Switching Protocols"


def websocket_connect(port):
    """Upgrade to WebSocket and return the still open socket, or None."""
    key = base64.b64encode(os.urandom(16)).decode()
    request = (
        "GET / HTTP/1.1\r\n"
        "Host: 127.0.0.1:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n" % (port, key)
    ).encode()
    conn = socket.create_connection(("127.0.0.1", port), timeout=5)
    conn.sendall(request)
    response = recv_until(conn, b"\r\n\r\n", 5)
    if not response.startswith(b"HTTP/1.1 101"):
        conn.close()
        return None
    return conn


def read_ws_frame(conn, timeout=5):
    """Read one server-to-client frame and return its payload, or None."""
    conn.settimeout(timeout)

    def exact(n):
        buf = b""
        while len(buf) < n:
            chunk = conn.recv(n - len(buf))
            if not chunk:
                return None
            buf += chunk
        return buf

    header = exact(2)
    if header is None:
        return None
    length = header[1] & 0x7F
    if length == 126:
        extra = exact(2)
        if extra is None:
            return None
        length = struct.unpack(">H", extra)[0]
    elif length == 127:
        extra = exact(8)
        if extra is None:
            return None
        length = struct.unpack(">Q", extra)[0]
    return exact(length) if length else b""


def main():
    if len(sys.argv) < 2:
        print("usage: server_e2e_test.py <LogServer> [webserver-type]")
        return 2

    binary = os.path.abspath(sys.argv[1])
    web_type = sys.argv[2] if len(sys.argv) > 2 else "SSE"
    failures = []

    def check(condition, message):
        if condition:
            print("PASS %s" % message)
        else:
            print("FAIL %s" % message)
            failures.append(message)

    workdir = tempfile.mkdtemp(prefix="logserver_e2e_")
    udp_port = free_port(socket.SOCK_DGRAM)
    tcp_port = free_port(socket.SOCK_STREAM)
    web_port = free_port(socket.SOCK_STREAM)

    args = [binary, "-u", str(udp_port), "-t", str(tcp_port), "-s", "5"]
    if web_type != "NONE":
        args += ["-w", str(web_port)]

    print("running: %s (cwd=%s, web=%s)" % (" ".join(args), workdir, web_type))
    log_path = os.path.join(workdir, "stdout.txt")
    with open(log_path, "wb") as stdout_file:
        process = subprocess.Popen(
            args,
            cwd=workdir,
            stdout=stdout_file,
            stderr=subprocess.STDOUT,
        )

    try:
        deadline = time.time() + TIMEOUT
        conn = wait_for_tcp(tcp_port, deadline)
        check(conn is not None, "TCP server accepts connections")
        if conn is None:
            return 1

        welcome = recv_until(conn, b"Welcome", 3)
        check(b"Welcome to LogServer" in welcome, "TCP client receives the welcome banner")

        udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        udp.sendto(b"e2e {braces} 100%\n", ("127.0.0.1", udp_port))
        forwarded = recv_until(conn, b"e2e", 3)
        check(b"e2e {braces} 100%\n" in forwarded, "UDP payload is fanned out to TCP unchanged")

        # A datagram larger than the old 1024 byte buffer must arrive whole.
        long_payload = b"LONG[" + (b"0123456789" * 400) + b"]END\n"  # 4006 bytes
        udp.sendto(long_payload, ("127.0.0.1", udp_port))
        long_forwarded = recv_until(conn, b"]END", 5)
        check(long_payload in long_forwarded,
              "a %d byte datagram is forwarded whole, not truncated" % len(long_payload))

        if web_type in ("SSE", "WEBSOCKET"):
            page = fetch_page(web_port)
            check(page.startswith(b"HTTP/1.1 200") and b"</html>" in page,
                  "web server serves the log page at /")

        if web_type == "SSE":
            sse = socket.create_connection(("127.0.0.1", web_port), timeout=5)
            sse.sendall(b"GET /sse HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n")
            headers = recv_until(sse, b"\r\n\r\n", 5)
            check(headers.startswith(b"HTTP/1.1 200"), "SSE endpoint returns 200")
            udp.sendto(b"sse-a\nsse-b\n", ("127.0.0.1", udp_port))
            payload = recv_until(sse, b"data: sse-b", 5)
            check(b"data: sse-a\ndata: sse-b\n" in payload,
                  "SSE receives one data: line per log line")
            check(b"\x00" not in headers + payload,
                  "SSE stream carries no NUL byte, including after the headers")

            # GB2312 must arrive as UTF-8: EventSource always decodes as UTF-8.
            udp.sendto(GB2312_CHINESE + b"\n", ("127.0.0.1", udp_port))
            gb_event = recv_until(sse, GB2312_CHINESE_UTF8, 5)
            check(GB2312_CHINESE_UTF8 in gb_event,
                  "SSE converts a GB2312 line to UTF-8 for the browser")
            try:
                gb_event.decode("utf-8")
                decodes = True
            except UnicodeDecodeError:
                decodes = False
            check(decodes, "SSE GB2312 event is valid UTF-8")
            sse.close()
        elif web_type == "WEBSOCKET":
            status = websocket_handshake(web_port)
            check(status.startswith(b"HTTP/1.1 101"),
                  "web server completes the WebSocket handshake")

            # A connected browser must receive every log line, whatever
            # encoding it arrives in. A text frame also has to be valid UTF-8.
            ws = websocket_connect(web_port)
            check(ws is not None, "WebSocket client connects for the payload checks")
            if ws is not None:
                # The server registers a new client through a queue, so give the
                # processing thread a moment before the first broadcast.
                time.sleep(0.3)
                udp.sendto(GB2312_CHINESE + b"\n", ("127.0.0.1", udp_port))
                frame = read_ws_frame(ws)
                check(frame is not None and GB2312_CHINESE_UTF8 in frame,
                      "WebSocket converts a GB2312 line to UTF-8")

                udp.sendto(INVALID_UTF8 + b"\n", ("127.0.0.1", udp_port))
                frame = read_ws_frame(ws)
                check(frame is not None and frame != b"",
                      "WebSocket delivers a line with undecodable bytes")
                if frame:
                    try:
                        frame.decode("utf-8")
                        decodes = True
                    except UnicodeDecodeError:
                        decodes = False
                    check(decodes, "WebSocket payload is valid UTF-8")
                    check(REPLACEMENT in frame,
                          "undecodable bytes are replaced with U+FFFD")
                ws.close()

        udp.close()
        conn.close()

        if sys.platform != "win32":
            process.send_signal(signal.SIGINT)
            try:
                exit_code = process.wait(timeout=TIMEOUT)
            except subprocess.TimeoutExpired:
                process.kill()
                exit_code = None
            check(exit_code == 0, "server exits cleanly on SIGINT (code=%s)" % exit_code)
        else:
            # Console control events are awkward from Python; just stop it.
            process.terminate()
            process.wait(timeout=TIMEOUT)

        # File logging is enabled only when the log directory is on a tmpfs,
        # and the server says so by printing the path it opened.
        if sys.platform == "win32":
            expect_files = True
        else:
            expect_files = is_tmpfs("/tmp")
        with open(log_path, "r", errors="replace") as stdout_file:
            output = stdout_file.read()
        wrote_files = "Log file path" in output
        check(wrote_files == expect_files,
              "file logging follows the tmpfs gate (expected=%s, actual=%s)"
              % (expect_files, wrote_files))
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()

    if failures:
        print("server output:")
        try:
            with open(log_path, "r", errors="replace") as stdout_file:
                print(stdout_file.read())
        except OSError:
            pass
        shutil.rmtree(workdir, ignore_errors=True)
        print("%d check(s) failed" % len(failures))
        return 1

    shutil.rmtree(workdir, ignore_errors=True)
    print("all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
