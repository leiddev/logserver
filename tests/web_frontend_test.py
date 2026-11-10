#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
#
# Leid 2026.11
#
"""
Stability, robustness and availability tests for the two web front ends.

Usage: web_frontend_test.py <path-to-LogServer> <SSE|WEBSOCKET>

The functional checks (fan-out, encodings, page) live in server_e2e_test.py.
This file attacks the parts that only show up over time or under load:

  * a client of the event stream that stops reading must neither stall the
    UDP/TCP fan-out nor keep the server from dropping it;
  * every event arrives whole and in order, heartbeats included;
  * several clients get every event, and one going away does not disturb the
    others;
  * malformed HTTP requests get 400 and the server keeps serving;
  * WebSocket clients get every broadcast, text frames are echoed, and
    connecting/disconnecting during a flood does not take the server down.

The slow-consumer and event-integrity checks failed against V0.8.6 and guard
the fixes made since; the rest cover behaviour that must not regress.
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
import threading
import time

TIMEOUT = 10.0


# ---------------------------------------------------------------------------
# Small socket helpers
# ---------------------------------------------------------------------------

def free_port(sock_type):
    sock = socket.socket(socket.AF_INET, sock_type)
    sock.bind(("127.0.0.1", 0))
    port = sock.getsockname()[1]
    sock.close()
    return port


def recv_until(sock, expected, timeout):
    sock.settimeout(timeout)
    data = b""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            chunk = sock.recv(65536)
        except socket.timeout:
            break
        except OSError:
            break
        if not chunk:
            break
        data += chunk
        if expected in data:
            break
    return data


def http_get(port, path="/"):
    conn = socket.create_connection(("127.0.0.1", port), timeout=5)
    conn.sendall(("GET %s HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n" % path).encode())
    response = recv_until(conn, b"\r\n\r\n", 5)
    conn.close()
    return response


# ---------------------------------------------------------------------------
# Server under test
# ---------------------------------------------------------------------------

class Server(object):
    def __init__(self, binary, web_type):
        self.web_type = web_type
        self.workdir = tempfile.mkdtemp(prefix="logserver_frontend_")
        self.udp_port = free_port(socket.SOCK_DGRAM)
        self.tcp_port = free_port(socket.SOCK_STREAM)
        self.web_port = free_port(socket.SOCK_STREAM)
        self.output_path = os.path.join(self.workdir, "stdout.txt")
        self.output_file = open(self.output_path, "wb")
        self.process = subprocess.Popen(
            [binary, "-u", str(self.udp_port), "-t", str(self.tcp_port),
             "-w", str(self.web_port), "-s", "0"],
            cwd=self.workdir,
            stdout=self.output_file,
            stderr=subprocess.STDOUT,
        )
        self.udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.udp_addr = ("127.0.0.1", self.udp_port)
        self.sockets = []

        conn = None
        deadline = time.time() + TIMEOUT
        while time.time() < deadline:
            try:
                conn = socket.create_connection(("127.0.0.1", self.tcp_port), timeout=1)
                break
            except OSError:
                time.sleep(0.05)
        if conn is None:
            raise RuntimeError("TCP server did not start")
        recv_until(conn, b"Welcome", 3)
        conn.close()

    def track(self, sock):
        self.sockets.append(sock)
        return sock

    def send_log(self, payload):
        self.udp.sendto(payload, self.udp_addr)

    def sse_connect(self, rcvbuf=None):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if rcvbuf is not None:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, rcvbuf)
        sock.settimeout(5)
        sock.connect(("127.0.0.1", self.web_port))
        sock.sendall(b"GET /sse HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n")
        self.track(sock)
        return sock

    def tcp_connect(self):
        """Connect a log consumer; None when the welcome banner is overdue."""
        sock = socket.create_connection(("127.0.0.1", self.tcp_port), timeout=5)
        self.track(sock)
        if b"Welcome" not in recv_until(sock, b"Welcome", 3):
            sock.close()
            return None
        return sock

    def ws_connect(self):
        key = base64.b64encode(os.urandom(16)).decode()
        request = (
            "GET / HTTP/1.1\r\n"
            "Host: 127.0.0.1:%d\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: %s\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n" % (self.web_port, key)
        ).encode()
        sock = socket.create_connection(("127.0.0.1", self.web_port), timeout=5)
        self.track(sock)
        sock.sendall(request)
        response = recv_until(sock, b"\r\n\r\n", 5)
        if not response.startswith(b"HTTP/1.1 101"):
            sock.close()
            return None
        return sock

    def alive(self):
        return self.process.poll() is None

    def output(self):
        self.output_file.flush()
        with open(self.output_path, "r", errors="replace") as stream:
            return stream.read()

    def stop(self):
        for sock in self.sockets:
            try:
                sock.close()
            except OSError:
                pass
        self.sockets = []
        self.udp.close()
        if self.process.poll() is None:
            if sys.platform != "win32":
                self.process.send_signal(signal.SIGINT)
                try:
                    self.process.wait(timeout=15)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait()
            else:
                self.process.terminate()
                self.process.wait(timeout=15)
        self.output_file.close()
        shutil.rmtree(self.workdir, ignore_errors=True)


# ---------------------------------------------------------------------------
# WebSocket frame helpers
# ---------------------------------------------------------------------------

def ws_send_text(conn, payload):
    mask = os.urandom(4)
    length = len(payload)
    header = b"\x81"  # FIN + text
    if length < 126:
        header += bytes([0x80 | length])
    elif length < 65536:
        header += bytes([0x80 | 126]) + struct.pack(">H", length)
    else:
        header += bytes([0x80 | 127]) + struct.pack(">Q", length)
    masked = bytes(byte ^ mask[i % 4] for i, byte in enumerate(payload))
    conn.sendall(header + mask + masked)


def ws_read_frame(conn, timeout=5):
    """Read one unmasked server frame, or None on timeout/EOF/close."""
    conn.settimeout(timeout)

    def exact(count):
        data = b""
        while len(data) < count:
            try:
                chunk = conn.recv(count - len(data))
            except socket.timeout:
                return None
            except OSError:
                return None
            if not chunk:
                return None
            data += chunk
        return data

    header = exact(2)
    if header is None:
        return None
    opcode = header[0] & 0x0F
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
    payload = exact(length) if length else b""
    if payload is None:
        return None
    if opcode == 0x8:  # close
        return None
    if opcode in (0x9, 0xA):  # ping/pong
        return ws_read_frame(conn, timeout)
    return payload


def ws_read_until(conn, predicate, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        payload = ws_read_frame(conn, max(0.01, deadline - time.time()))
        if payload is None:
            return None
        if predicate(payload):
            return payload
    return None


# ---------------------------------------------------------------------------
# SSE checks
# ---------------------------------------------------------------------------

def sse_headers_ok(ctx, sock, label):
    headers = recv_until(sock, b"\r\n\r\n", 5)
    ctx.check(headers.startswith(b"HTTP/1.1 200"), "%s: the event stream answers 200" % label)
    return headers


def check_sse_fanout(ctx):
    clients = []
    for index in range(3):
        sock = ctx.server.sse_connect()
        sse_headers_ok(ctx, sock, "SSE client %d" % index)
        clients.append(sock)
    time.sleep(0.1)
    ctx.server.send_log(b"FANOUT-A\nFANOUT-B\n")
    for index, sock in enumerate(clients):
        data = recv_until(sock, b"data: FANOUT-B", 5)
        ctx.check(b"data: FANOUT-A\n" in data and b"data: FANOUT-B\n" in data,
                  "SSE client %d receives every event" % index)
        sock.close()


def sse_payloads(data):
    """Split an event stream into the payload of each data: block, heartbeats aside."""
    payloads = []
    for block in data.split(b"\n\n"):
        lines = [line[len(b"data: "):] for line in block.split(b"\n") if line.startswith(b"data: ")]
        if not lines or lines == [b"[heartbeat]"]:
            continue
        payloads.append(b"\n".join(lines))
    return payloads


def check_sse_stalled_consumer(ctx):
    """A browser that stops reading must not stall the UDP/TCP fan-out.

    A client that never reads fills its send buffer, then its queue. It has to
    be dropped on its own, on the io_context thread, while the thread that
    receives the datagrams keeps accepting clients and fanning logs out.
    """
    stalled = ctx.server.sse_connect(rcvbuf=2048)
    sse_headers_ok(ctx, stalled, "stalled SSE client")

    # 320 events of 60 KB: the kernel buffers take a few megabytes, the rest
    # piles up in the client's queue until the 4 MB limit drops it.
    for index in range(320):
        body = b"STALL-%03d " % index
        body += b"x" * (60000 - len(body))
        ctx.server.send_log(body + b"\n")
        time.sleep(0.0002)
    time.sleep(0.3)

    # A consumer that connects after the flood has none of it queued ahead of
    # its own data, so its answer measures the IO thread and nothing else.
    probe = ctx.server.tcp_connect()
    ctx.check(probe is not None, "a TCP client is accepted while the stalled stream is blocked")
    if probe is not None:
        ctx.server.send_log(b"AFTER-FLOOD\n")
        data = recv_until(probe, b"AFTER-FLOOD", 2.0)
        ctx.check(b"AFTER-FLOOD" in data,
                  "a stalled SSE client does not stall the IO thread "
                  "(UDP reception and the TCP fan-out answer within 2s)")
        probe.close()

    evicted = stalled_closed(stalled, 10.0)
    ctx.check(evicted, "the server drops the SSE client that stopped reading")
    ctx.check(ctx.server.alive(), "the server survives a stalled SSE client")

    # The stream keeps working for the next client.
    fresh = ctx.server.sse_connect()
    sse_headers_ok(ctx, fresh, "SSE client after the eviction")
    ctx.server.send_log(b"AFTER-EVICTION\n")
    data = recv_until(fresh, b"data: AFTER-EVICTION", 5)
    ctx.check(b"data: AFTER-EVICTION\n" in data,
              "a new SSE client is served after the stalled one was dropped")
    fresh.close()


def stalled_closed(sock, timeout):
    """Drain a client until the server closes its side, then report whether it did."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        sock.settimeout(min(0.5, max(0.01, deadline - time.time())))
        try:
            chunk = sock.recv(65536)
        except socket.timeout:
            continue
        except OSError:
            return True  # reset by the server
        if not chunk:
            return True  # orderly close
    return False


def check_sse_event_integrity(ctx):
    """Every event arrives whole and in order, with no bytes lost or spliced."""
    sock = ctx.server.sse_connect()
    sse_headers_ok(ctx, sock, "SSE ordered stream")

    bodies = []
    for index in range(64):
        body = b"SEQ-%03d " % index
        body += b"y" * (4096 - len(body))
        bodies.append(body)
        ctx.server.send_log(body + b"\n")
        time.sleep(0.002)

    data = recv_until(sock, b"data: " + bodies[-1] + b"\n", 15)
    payloads = sse_payloads(data)
    ctx.check(payloads == bodies,
              "64 events arrive complete and in order (%d of %d payloads)"
              % (len(payloads), len(bodies)))
    ctx.check(b"\x00" not in data, "the event stream carries no NUL byte")
    sock.close()


def check_sse_abrupt_disconnect(ctx):
    keep = ctx.server.sse_connect()
    sse_headers_ok(ctx, keep, "SSE survivor")
    leaving = ctx.server.sse_connect()
    sse_headers_ok(ctx, leaving, "SSE leaver")
    time.sleep(0.1)

    ctx.server.send_log(b"BEFORE-DROP\n")
    data = recv_until(keep, b"data: BEFORE-DROP", 5)
    ctx.check(b"data: BEFORE-DROP\n" in data, "the surviving client receives events before the drop")

    leaving.close()
    time.sleep(0.1)
    ctx.server.send_log(b"AFTER-DROP\n")
    data = recv_until(keep, b"data: AFTER-DROP", 5)
    ctx.check(b"data: AFTER-DROP\n" in data,
              "a client that disappears mid-stream does not disturb the others")
    ctx.check(ctx.server.alive(), "the server survives an abrupt SSE disconnect")
    keep.close()


def check_sse_large_line(ctx):
    sock = ctx.server.sse_connect()
    sse_headers_ok(ctx, sock, "SSE large line")
    body = b"LARGE-" + b"z" * 60000
    ctx.server.send_log(body + b"\n")
    expected = b"data: " + body + b"\n"
    data = recv_until(sock, expected, 5)
    ctx.check(expected in data, "a 60 KB line arrives as one intact event")
    sock.close()


def check_sse_malformed_requests(ctx):
    cases = [
        (b"POST /sse HTTP/1.1\r\nHost: x\r\n\r\n", "non-GET"),
        (b"not-http\r\n\r\n", "garbage"),
        (b"\r\n\r\n", "empty request line"),
    ]
    for request, label in cases:
        conn = socket.create_connection(("127.0.0.1", ctx.server.web_port), timeout=5)
        conn.sendall(request)
        response = recv_until(conn, b"\r\n\r\n", 3)
        conn.close()
        ctx.check(response.startswith(b"HTTP/1.1 400"),
                  "SSE %s request is answered with 400" % label)
    page = http_get(ctx.server.web_port)
    ctx.check(page.startswith(b"HTTP/1.1 200"),
              "the SSE server still serves the log page after malformed requests")


# ---------------------------------------------------------------------------
# WebSocket checks
# ---------------------------------------------------------------------------

def check_ws_fanout(ctx):
    clients = []
    for _ in range(3):
        sock = ctx.server.ws_connect()
        ctx.check(sock is not None, "WebSocket client connects")
        if sock is not None:
            clients.append(sock)
    time.sleep(0.3)
    expected = b"WS-FANOUT-1\nWS-FANOUT-2\n"
    ctx.server.send_log(expected)
    for index, sock in enumerate(clients):
        payload = ws_read_until(sock, lambda frame: frame == expected, 5)
        ctx.check(payload == expected, "WebSocket client %d receives the whole line" % index)
        sock.close()


def check_ws_echo(ctx):
    sock = ctx.server.ws_connect()
    ctx.check(sock is not None, "WebSocket echo client connects")
    if sock is None:
        return
    time.sleep(0.2)
    ws_send_text(sock, b"echo-payload")
    payload = ws_read_until(sock, lambda frame: frame == b"echo-payload", 5)
    ctx.check(payload == b"echo-payload", "a text frame is echoed back")
    sock.close()


def check_ws_churn(ctx):
    """Connect and disconnect clients while log lines flood in.

    This is the end-to-end side of the m_connections race: broadcast_message()
    reads the subscriber set from the UDP thread while the processing thread
    inserts and erases from it. Reproducing the undefined behaviour itself
    needs ThreadSanitizer (websocket_broadcast_race_test); here we check that
    the server stays up and keeps delivering.
    """
    stable = ctx.server.ws_connect()
    ctx.check(stable is not None, "the stable WebSocket client connects")
    if stable is None:
        return
    time.sleep(0.3)

    stop = threading.Event()

    def flood():
        index = 0
        while not stop.is_set():
            ctx.server.send_log(b"CHURN-%06d\n" % index)
            index += 1
            time.sleep(0.001)

    flooder = threading.Thread(target=flood)
    flooder.start()
    opened = 0
    try:
        for _ in range(40):
            sock = ctx.server.ws_connect()
            if sock is not None:
                opened += 1
                sock.close()
    finally:
        stop.set()
        flooder.join()

    ctx.check(opened == 40, "40 of 40 churn clients complete the handshake")
    ctx.server.send_log(b"AFTER-CHURN\n")
    payload = ws_read_until(stable, lambda frame: b"AFTER-CHURN" in frame, 10)
    ctx.check(payload is not None, "the stable client still receives broadcasts after the churn")
    ctx.check(ctx.server.alive(), "the server survives connect/disconnect churn during a flood")
    page = http_get(ctx.server.web_port)
    ctx.check(page.startswith(b"HTTP/1.1 200"), "the WebSocket server still serves the log page")
    stable.close()


def check_ws_bad_handshake(ctx):
    conn = socket.create_connection(("127.0.0.1", ctx.server.web_port), timeout=5)
    conn.sendall(b"GET / HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n\r\n")
    response = recv_until(conn, b"\r\n\r\n", 3)
    conn.close()
    ctx.check(not response.startswith(b"HTTP/1.1 101"),
              "an incomplete upgrade request is not accepted")
    sock = ctx.server.ws_connect()
    ctx.check(sock is not None, "the server still accepts a valid handshake")
    if sock is not None:
        sock.close()


# ---------------------------------------------------------------------------
# Runner
# ---------------------------------------------------------------------------

class Context(object):
    def __init__(self, server):
        self.server = server
        self.failures = []

    def check(self, condition, message):
        if condition:
            print("PASS %s" % message)
        else:
            print("FAIL %s" % message)
            self.failures.append(message)


def main():
    if len(sys.argv) < 3:
        print("usage: web_frontend_test.py <LogServer> <SSE|WEBSOCKET>")
        return 2

    binary = os.path.abspath(sys.argv[1])
    web_type = sys.argv[2].upper()
    if web_type not in ("SSE", "WEBSOCKET"):
        print("unknown web server type: %s" % web_type)
        return 2

    server = Server(binary, web_type)
    ctx = Context(server)
    print("running %s front end tests on ports udp=%d tcp=%d web=%d"
          % (web_type, server.udp_port, server.tcp_port, server.web_port))

    try:
        if web_type == "SSE":
            check_sse_fanout(ctx)
            check_sse_stalled_consumer(ctx)
            check_sse_event_integrity(ctx)
            check_sse_abrupt_disconnect(ctx)
            check_sse_large_line(ctx)
            check_sse_malformed_requests(ctx)
        else:
            check_ws_fanout(ctx)
            check_ws_echo(ctx)
            check_ws_churn(ctx)
            check_ws_bad_handshake(ctx)
    finally:
        output = server.output()
        server.stop()

    if ctx.failures:
        print("server output:")
        print(output)
        print("%d check(s) failed" % len(ctx.failures))
        return 1

    print("all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
