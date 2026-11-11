#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
#
# Leid 2026.11
#
"""
Stability and reliability tests for the UDP producer and the TCP consumer.

Usage: transport_stability_test.py <path-to-LogServer> <NONE|SSE|WEBSOCKET>

server_e2e_test.py only checks that one datagram reaches one TCP client, and
web_frontend_test.py uses UDP as a log source while it measures the web front
ends. Neither looks at the transport itself. This file does:

  * UDP: a burst of sequenced datagrams (order, integrity, loss budget), the
    largest IPv4 datagram, an empty datagram, several producers at once, and
    byte exact payloads;
  * TCP: fan-out to many consumers, a consumer that stops reading being
    dropped instead of growing the server, an abrupt disconnect, connect and
    disconnect churn during a flood, the echo of a fragmented write, and the
    shutdown path with clients still connected.

Everything runs against the executable, like the other end-to-end tests, and
the log file is disabled (-s 0) so a flood cannot end up on disk.
"""

import re
import os
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time

TIMEOUT = 10.0

# The largest payload an IPv4 UDP datagram can carry: 65535 minus the IP and
# UDP headers. Udp_server sizes its receive buffer to cover it.
MAX_DATAGRAM = 65507

BURST_RE = re.compile(br"^BURST-(\d{4}) ")


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


def burst_payload(index):
    """One 1 KB datagram body carrying its sequence number, without the newline."""
    head = b"BURST-%04d " % index
    return head + b"b" * (1024 - len(head) - 1)


class Collector(object):
    """Reads one TCP consumer in a background thread.

    A test that sends before it reads would otherwise stall its own consumer,
    and Tcp_server drops a consumer whose queue grows past its limit. Reading
    from a thread keeps the test measuring the server instead of itself.
    """

    def __init__(self, sock):
        self.sock = sock
        self.sock.settimeout(0.2)
        self.buffer = bytearray()
        self.lock = threading.Lock()
        self.finished = threading.Event()
        self.thread = threading.Thread(target=self._read)
        self.thread.daemon = True
        self.thread.start()

    def _read(self):
        while not self.finished.is_set():
            try:
                chunk = self.sock.recv(65536)
            except socket.timeout:
                continue
            except OSError:
                break
            if not chunk:
                break
            with self.lock:
                self.buffer += chunk

    def snapshot(self):
        with self.lock:
            return bytes(self.buffer)

    def lines(self):
        """The complete newline terminated lines received so far."""
        return self.snapshot().split(b"\n")[:-1]

    def wait_for(self, needle, timeout):
        deadline = time.time() + timeout
        while time.time() < deadline:
            data = self.snapshot()
            if needle in data:
                return data
            time.sleep(0.02)
        return self.snapshot()

    def stop(self):
        self.finished.set()
        self.thread.join()


# ---------------------------------------------------------------------------
# Server under test
# ---------------------------------------------------------------------------

class Server(object):
    def __init__(self, binary, web_type):
        self.web_type = web_type
        self.workdir = tempfile.mkdtemp(prefix="logserver_transport_")
        self.udp_port = free_port(socket.SOCK_DGRAM)
        self.tcp_port = free_port(socket.SOCK_STREAM)
        self.web_port = free_port(socket.SOCK_STREAM)
        self.output_path = os.path.join(self.workdir, "stdout.txt")
        self.output_file = open(self.output_path, "wb")
        self.sockets = []

        args = [binary, "-u", str(self.udp_port), "-t", str(self.tcp_port), "-s", "0"]
        if web_type != "NONE":
            args += ["-w", str(self.web_port)]

        self.process = subprocess.Popen(
            args,
            cwd=self.workdir,
            stdout=self.output_file,
            stderr=subprocess.STDOUT,
        )

        self.udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            # Room for the largest datagram plus whatever is queued behind it.
            self.udp.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 1 << 20)
        except OSError:
            pass
        self.udp_addr = ("127.0.0.1", self.udp_port)

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
        conn.close()

    def track(self, sock):
        self.sockets.append(sock)
        return sock

    def send(self, payload):
        """Send one datagram; a closed server is not an error here."""
        try:
            self.udp.sendto(payload, self.udp_addr)
        except OSError:
            pass

    def tcp_connect(self):
        """Connect a log consumer and consume its welcome banner.

        The timeout is generous on purpose: a box running the rest of the suite
        at the same time accepts a connection late now and then, and that is not
        the behaviour under test here.
        """
        try:
            sock = socket.create_connection(("127.0.0.1", self.tcp_port), timeout=10)
        except OSError:
            return None
        self.track(sock)
        banner = recv_until(sock, b"\n", 5)
        if b"Welcome" not in banner:
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
# UDP producer checks
# ---------------------------------------------------------------------------

def check_udp_burst_lossless(ctx):
    """A burst of sequenced datagrams: order, integrity, and a loss budget."""
    consumer = ctx.server.tcp_connect()
    ctx.check(consumer is not None, "a TCP consumer connects for the burst")
    if consumer is None:
        return
    collector = Collector(consumer)

    total = 1000
    for index in range(total):
        ctx.server.send(burst_payload(index) + b"\n")
        if (index + 1) % 100 == 0:
            time.sleep(0.005)

    deadline = time.time() + 20
    while time.time() < deadline and len(collector.lines()) < total:
        time.sleep(0.05)
    lines = collector.lines()
    collector.stop()
    consumer.close()

    received = []
    malformed = 0
    for line in lines:
        match = BURST_RE.match(line)
        if match is None:
            malformed += 1
            continue
        index = int(match.group(1))
        if index >= total or line != burst_payload(index):
            malformed += 1
            continue
        received.append(index)

    lost = total - len(received)
    ctx.check(lost <= total // 100,
              "at most 1%% of %d burst datagrams are lost (received %d, lost %d)"
              % (total, len(received), lost))
    ctx.check(malformed == 0, "every received burst line is intact (malformed %d)" % malformed)
    ctx.check(received == sorted(received) and len(set(received)) == len(received),
              "burst lines arrive in order and exactly once")


def check_udp_max_datagram(ctx):
    """The largest IPv4 payload must arrive whole, not truncated by the buffer."""
    consumer = ctx.server.tcp_connect()
    ctx.check(consumer is not None, "a TCP consumer connects for the maximum datagram")
    if consumer is None:
        return
    collector = Collector(consumer)

    body = b"MAXDAT[" + b"m" * (MAX_DATAGRAM - len(b"MAXDAT[") - 2) + b"]\n"
    assert len(body) == MAX_DATAGRAM
    try:
        ctx.server.udp.sendto(body, ctx.server.udp_addr)
    except OSError as exc:
        # Some hosts refuse a datagram this large; that is a platform fact, not
        # a server failure, and the truncation path cannot be reached then.
        ctx.skip("the host refuses a %d byte datagram: %s" % (MAX_DATAGRAM, exc))
        collector.stop()
        consumer.close()
        return

    data = collector.wait_for(b"]\n", 10)
    collector.stop()
    consumer.close()
    ctx.check(body in data,
              "a %d byte datagram (IPv4 maximum) is forwarded whole" % MAX_DATAGRAM)


def check_udp_empty_datagram(ctx):
    """A zero byte datagram is not a log line and must not be an error either."""
    consumer = ctx.server.tcp_connect()
    ctx.check(consumer is not None, "a TCP consumer connects for the empty datagram")
    if consumer is None:
        return
    collector = Collector(consumer)
    time.sleep(0.1)

    before = len(collector.snapshot())
    ctx.server.send(b"")
    time.sleep(0.3)
    after = len(collector.snapshot())
    ctx.check(after == before,
              "an empty datagram produces no output for the TCP consumer (%d bytes appeared)"
              % (after - before))

    ctx.server.send(b"AFTER-EMPTY\n")
    ctx.check(b"AFTER-EMPTY\n" in collector.wait_for(b"AFTER-EMPTY", 5),
              "the server keeps receiving after an empty datagram")
    collector.stop()
    consumer.close()


def check_udp_multi_source(ctx):
    """Several producers reach the same consumer; the server keeps no per source state."""
    consumer = ctx.server.tcp_connect()
    ctx.check(consumer is not None, "a TCP consumer connects for the multi source check")
    if consumer is None:
        return
    collector = Collector(consumer)

    senders = []
    try:
        for index in range(3):
            sender = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sender.bind(("127.0.0.1", 0))
            senders.append(sender)
            sender.sendto(b"SRC-%d\n" % index, ctx.server.udp_addr)
        data = collector.wait_for(b"SRC-2", 5)
    finally:
        for sender in senders:
            sender.close()

    for index in range(3):
        ctx.check((b"SRC-%d\n" % index) in data,
                  "the line from producer %d arrives at the consumer" % index)
    collector.stop()
    consumer.close()


def check_udp_payload_fidelity(ctx):
    """Any byte may appear in a datagram: NUL, CRLF, braces, percent signs."""
    consumer = ctx.server.tcp_connect()
    ctx.check(consumer is not None, "a TCP consumer connects for the payload fidelity check")
    if consumer is None:
        return
    collector = Collector(consumer)

    body = b"FID\x00A\r\nB%%{}C" + b"p" * 4000 + b"\n"
    ctx.server.send(body)
    data = collector.wait_for(body, 5)
    collector.stop()
    consumer.close()

    ctx.check(body in data, "a payload with NUL, CRLF and format characters arrives byte for byte")
    ctx.check(data.count(b"FID") == 1, "the payload is forwarded once, not repeated or spliced")
    ctx.check(len(data) == len(body), "nothing is added to or removed from the payload")


# ---------------------------------------------------------------------------
# TCP consumer checks
# ---------------------------------------------------------------------------

def check_tcp_fanout(ctx):
    """Every consumer gets every line, in datagram order, however many there are."""
    clients = []
    for index in range(3):
        sock = ctx.server.tcp_connect()
        ctx.check(sock is not None, "TCP consumer %d connects" % index)
        if sock is not None:
            clients.append(sock)

    time.sleep(0.1)
    ctx.server.send(b"FANOUT-1\nFANOUT-2\n")
    for index, sock in enumerate(clients):
        data = recv_until(sock, b"FANOUT-2", 5)
        ctx.check(b"FANOUT-1\n" in data and b"FANOUT-2\n" in data,
                  "TCP consumer %d receives every line of a multi line datagram" % index)
        first = data.find(b"FANOUT-1")
        second = data.find(b"FANOUT-2")
        ctx.check(0 <= first < second, "TCP consumer %d keeps the line order" % index)
    for sock in clients:
        sock.close()

    many = []
    for _ in range(50):
        sock = ctx.server.tcp_connect()
        if sock is not None:
            many.append(sock)
    ctx.check(len(many) == 50, "50 concurrent consumers connect (%d of 50)" % len(many))

    time.sleep(0.2)
    ctx.server.send(b"MANY-CLIENTS\n")
    missing = 0
    for sock in many:
        if b"MANY-CLIENTS\n" not in recv_until(sock, b"MANY-CLIENTS", 10):
            missing += 1
    ctx.check(missing == 0, "all 50 concurrent consumers receive the line (missing %d)" % missing)
    for sock in many:
        sock.close()


def check_tcp_slow_consumer(ctx):
    """A consumer that stops reading is dropped instead of growing the server.

    The session queue here used to be unbounded, so a consumer that never read
    made the process grow with every log line. The check floods far past the
    per client limit and expects that one connection to be closed while the
    rest of the service keeps working.
    """
    stalled = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    stalled.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 2048)
    stalled.settimeout(5)
    try:
        stalled.connect(("127.0.0.1", ctx.server.tcp_port))
    except OSError:
        ctx.check(False, "the slow consumer connects")
        stalled.close()
        return
    ctx.server.track(stalled)
    ctx.check(True, "the slow consumer connects")

    # 800 datagrams of 60 KB: far past the 4 MB per client limit even when the
    # kernel drops most of them on the receive path.
    for index in range(800):
        body = b"TCP-STALL-%04d " % index
        body += b"x" * (60000 - len(body))
        ctx.server.send(body + b"\n")
        time.sleep(0.0002)
    time.sleep(0.3)

    # A consumer that connects after the flood has none of it queued ahead of
    # its own data, so its answer measures the IO thread and nothing else.
    probe = ctx.server.tcp_connect()
    ctx.check(probe is not None, "a TCP consumer is accepted while the stalled one is blocked")
    if probe is not None:
        ctx.server.send(b"AFTER-TCP-FLOOD\n")
        data = recv_until(probe, b"AFTER-TCP-FLOOD", 5.0)
        ctx.check(b"AFTER-TCP-FLOOD" in data,
                  "a stalled TCP consumer does not stall the IO thread "
                  "(UDP reception and the fan-out answer within 5s)")
        probe.close()

    ctx.check(stalled_closed(stalled, 15.0),
              "the server drops the TCP consumer that stopped reading")
    ctx.check(ctx.server.alive(), "the server survives a stalled TCP consumer")

    fresh = ctx.server.tcp_connect()
    ctx.check(fresh is not None, "a TCP consumer connects after the eviction")
    if fresh is not None:
        ctx.server.send(b"AFTER-TCP-EVICTION\n")
        data = recv_until(fresh, b"AFTER-TCP-EVICTION", 5)
        ctx.check(b"AFTER-TCP-EVICTION\n" in data,
                  "a new TCP consumer is served after the stalled one was dropped")
        fresh.close()


def check_tcp_abrupt_disconnect(ctx):
    """One consumer disappearing must not disturb the others."""
    keep = ctx.server.tcp_connect()
    ctx.check(keep is not None, "the surviving TCP consumer connects")
    if keep is None:
        return
    leaving = ctx.server.tcp_connect()
    ctx.check(leaving is not None, "the leaving TCP consumer connects")
    if leaving is None:
        keep.close()
        return

    ctx.server.send(b"BEFORE-TCP-DROP\n")
    ctx.check(b"BEFORE-TCP-DROP\n" in recv_until(keep, b"BEFORE-TCP-DROP", 5),
              "the surviving consumer receives lines before the drop")

    leaving.close()
    time.sleep(0.2)
    ctx.server.send(b"AFTER-TCP-DROP\n")
    ctx.check(b"AFTER-TCP-DROP\n" in recv_until(keep, b"AFTER-TCP-DROP", 5),
              "a consumer that disappears mid-stream does not disturb the others")
    ctx.check(ctx.server.alive(), "the server survives an abrupt TCP disconnect")
    keep.close()


def check_tcp_churn(ctx):
    """Connect and disconnect consumers while log lines flood in."""
    stable = ctx.server.tcp_connect()
    ctx.check(stable is not None, "the stable TCP consumer connects")
    if stable is None:
        return

    stop = threading.Event()

    def flood():
        index = 0
        while not stop.is_set():
            ctx.server.send(b"TCP-CHURN-%06d\n" % index)
            index += 1
            time.sleep(0.001)

    flooder = threading.Thread(target=flood)
    flooder.start()
    opened = 0
    try:
        for _ in range(40):
            sock = ctx.server.tcp_connect()
            if sock is not None:
                opened += 1
                sock.close()
    finally:
        stop.set()
        flooder.join()

    ctx.check(opened == 40, "40 of 40 churn consumers complete the handshake")
    time.sleep(0.2)
    ctx.server.send(b"AFTER-TCP-CHURN\n")
    ctx.check(b"AFTER-TCP-CHURN\n" in recv_until(stable, b"AFTER-TCP-CHURN", 10),
              "the stable consumer still receives lines after the churn")
    ctx.check(ctx.server.alive(), "the server survives connect/disconnect churn during a flood")
    stable.close()


def check_tcp_fragmented_echo(ctx):
    """A write split over several segments is echoed back as the original bytes."""
    sender = ctx.server.tcp_connect()
    ctx.check(sender is not None, "the echoing consumer connects")
    if sender is None:
        return
    observer = ctx.server.tcp_connect()
    ctx.check(observer is not None, "a bystander consumer connects")
    if observer is None:
        sender.close()
        return

    payload = b"FRAGMENTED-ECHO-MARKER\n"
    try:
        sender.sendall(payload[:7])
        time.sleep(0.05)
        sender.sendall(payload[7:13])
        time.sleep(0.05)
        sender.sendall(payload[13:])
    except OSError:
        ctx.check(False, "the fragmented write reaches the server")
        sender.close()
        observer.close()
        return

    data = recv_until(sender, payload, 5)
    ctx.check(payload in data, "the echo of a fragmented write is the original line")
    ctx.check(data.count(payload) == 1, "the echo is sent once, not once per segment")

    time.sleep(0.2)
    bystander = recv_until(observer, payload, 0.5)
    ctx.check(payload not in bystander, "an echo goes only to the consumer that sent it")
    sender.close()
    observer.close()


def check_shutdown_with_live_clients(ctx):
    """SIGINT with consumers still connected: the sessions are closed and it exits."""
    clients = []
    for index in range(2):
        sock = ctx.server.tcp_connect()
        if sock is not None:
            clients.append(sock)
    ctx.check(len(clients) == 2, "two consumers stay connected for the shutdown (%d of 2)"
              % len(clients))

    stop = threading.Event()

    def flood():
        index = 0
        while not stop.is_set():
            ctx.server.send(b"SHUTDOWN-FLOOD-%06d\n" % index)
            index += 1
            time.sleep(0.002)

    flooder = threading.Thread(target=flood)
    flooder.start()
    time.sleep(0.2)

    try:
        if sys.platform != "win32":
            ctx.server.process.send_signal(signal.SIGINT)
            try:
                code = ctx.server.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                ctx.server.process.kill()
                ctx.server.process.wait()
                code = None
            ctx.check(code == 0,
                      "the server exits with code 0 on SIGINT with consumers connected "
                      "(code=%s)" % code)
        else:
            # Console control events are awkward from Python; just stop it.
            ctx.server.process.terminate()
            try:
                ctx.server.process.wait(timeout=10)
                exited = True
            except subprocess.TimeoutExpired:
                ctx.server.process.kill()
                ctx.server.process.wait()
                exited = False
            ctx.check(exited, "the server terminates with consumers connected")
    finally:
        stop.set()
        flooder.join()

    ctx.check(not ctx.server.alive(), "the server is gone after the shutdown")
    for sock in clients:
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

    def skip(self, message):
        print("SKIP %s" % message)


def main():
    if len(sys.argv) < 2:
        print("usage: transport_stability_test.py <LogServer> [NONE|SSE|WEBSOCKET]")
        return 2

    binary = os.path.abspath(sys.argv[1])
    web_type = sys.argv[2].upper() if len(sys.argv) > 2 else "SSE"
    if web_type not in ("NONE", "SSE", "WEBSOCKET"):
        print("unknown web server type: %s" % web_type)
        return 2

    server = Server(binary, web_type)
    ctx = Context(server)
    print("running transport tests on ports udp=%d tcp=%d web=%d (web=%s)"
          % (server.udp_port, server.tcp_port, server.web_port, web_type))

    try:
        check_udp_burst_lossless(ctx)
        check_udp_max_datagram(ctx)
        check_udp_empty_datagram(ctx)
        check_udp_multi_source(ctx)
        check_udp_payload_fidelity(ctx)
        check_tcp_fanout(ctx)
        check_tcp_slow_consumer(ctx)
        check_tcp_abrupt_disconnect(ctx)
        check_tcp_churn(ctx)
        check_tcp_fragmented_echo(ctx)
        # Last: it stops the server.
        check_shutdown_with_live_clients(ctx)
    finally:
        output = server.output()
        server.stop()

    # Nothing that happens above is allowed to show up as a transport error.
    ctx.check("UDP receive error" not in output,
              "the server reports no UDP receive error during the run")
    ctx.check("UDP datagram of" not in output,
              "no datagram reported as filling the receive buffer")
    ctx.check("Log server exception" not in output,
              "the server logged no uncaught exception")

    if ctx.failures:
        print("server output:")
        print(output)
        print("%d check(s) failed" % len(ctx.failures))
        return 1

    print("all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
