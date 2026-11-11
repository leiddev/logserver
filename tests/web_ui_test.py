#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
#
# Leid 2026.11
#
"""
Browser tests for the log viewer page.

Usage: web_ui_test.py <path-to-LogServer> <SSE|WEBSOCKET>

server_e2e_test.py and web_frontend_test.py check what the server sends over
HTTP, SSE and WebSocket; none of them ever renders the page. This file drives
the compiled-in page in a real Chromium, so the rendering, batching, trimming,
clearing, saving, auto-scroll and reconnect code runs for real:

  * the status text, the state dot and the connect/stop button follow the
    connection through its states, and a manual stop is never retried;
  * a log line is rendered verbatim, HTML in a line is text and nothing else,
    and a 60 KB line arrives whole;
  * 清空 drops the lines already rendered and the flush still pending;
  * the box never holds more than 10000 lines, and 100000 lines leave both the
    DOM and the JavaScript heap flat;
  * 自动滚动 follows the newest line, holds the view while it is off, and
    resumes when it is turned back on;
  * 保存 exports the rendered lines, including the ones not flushed yet, under
    a timestamped name;
  * 关于 names the transport the page was built with;
  * a server that dies is retried three times and then left alone.

Needs Playwright and its Chromium:
    pip install playwright
    python -m playwright install chromium

The test is opt-in (LOG_SERVER_UI_TESTS=ON) because that browser is a large
download; CI installs it and turns the option on for SSE and WebSocket.
"""

import os
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import time

try:
    from playwright.sync_api import Error as PlaywrightError
    from playwright.sync_api import sync_playwright
except ImportError:
    print("playwright is not installed: pip install playwright && python -m playwright install chromium")
    sys.exit(2)

TIMEOUT = 10.0
TRANSPORT_NAMES = {"SSE": "SSE", "WEBSOCKET": "WebSocket"}

# The SSE front end splits a datagram into events and drops the line ending;
# the WebSocket front end forwards the datagram verbatim, newline included.
# Compare rendered lines without that trailing newline, which the page keeps
# as text (it adds no height, so it is not a rendering difference).
LINE_TEXT = ("((element) => { const text = element.textContent;"
             " return text.endsWith('\\n') ? text.slice(0, -1) : text; })")


# ---------------------------------------------------------------------------
# Server under test
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


class Server(object):
    def __init__(self, binary, web_type):
        self.workdir = tempfile.mkdtemp(prefix="logserver_ui_")
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

    def send_log(self, payload):
        self.udp.sendto(payload, self.udp_addr)

    def send_lines(self, prefix, count, start=0):
        """Send numbered lines, paced so neither front end drops any of them.

        50 lines every 4 ms stays well under the WebSocket action queue and the
        per-client byte limits, so a line that does not show up is a page bug
        rather than transport back pressure.
        """
        for index in range(start, start + count):
            self.send_log(("%s-%05d\n" % (prefix, index)).encode())
            if (index - start) % 50 == 49:
                time.sleep(0.004)

    def kill(self):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait(timeout=10)

    def output(self):
        self.output_file.flush()
        with open(self.output_path, "r", errors="replace") as stream:
            return stream.read()

    def stop(self):
        self.udp.close()
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait(timeout=10)
        self.output_file.close()
        shutil.rmtree(self.workdir, ignore_errors=True)


# ---------------------------------------------------------------------------
# The page under test
# ---------------------------------------------------------------------------

class Ui(object):
    def __init__(self, context, port):
        self.page = context.new_page()
        self.errors = []
        self.page.on("pageerror", lambda error: self.errors.append(str(error)))

        url = "http://127.0.0.1:%d/" % port
        deadline = time.time() + TIMEOUT
        while True:
            try:
                self.page.goto(url, wait_until="load", timeout=3000)
                return
            except PlaywrightError:
                if time.time() > deadline:
                    raise
                time.sleep(0.1)

    # -- state ------------------------------------------------------------

    def status(self):
        return self.page.text_content("#status")

    def state(self):
        return self.page.get_attribute("#status", "data-state")

    def wait_status(self, text, timeout=15):
        self.page.wait_for_function(
            "expected => document.getElementById('status').textContent === expected",
            arg=text, timeout=timeout * 1000)

    def toggle_class(self):
        return self.page.get_attribute("#toggleConnectBtn", "class")

    def connect(self):
        self.page.click("#toggleConnectBtn")
        self.wait_status("已连接")
        # The WebSocket server registers the client on its processing thread,
        # which the 101 response can beat; lines sent in that window reach
        # nobody. Give the subscription time to land before sending.
        time.sleep(0.3)

    def disconnect(self):
        self.page.click("#toggleConnectBtn")
        self.wait_status("已断开")

    # -- log lines --------------------------------------------------------

    def lines(self):
        return self.page.eval_on_selector_all(
            "#log-box > div", "elements => elements.map(%s)" % LINE_TEXT)

    def line_count(self):
        return self.page.evaluate("document.getElementById('log-box').children.length")

    def wait_for_line(self, text, timeout=15):
        self.page.wait_for_function(
            "expected => Array.from(document.querySelectorAll('#log-box > div'))"
            ".some(element => %s(element) === expected)" % LINE_TEXT,
            arg=text, timeout=timeout * 1000)

    def wait_for_text(self, text, timeout=15):
        self.page.wait_for_function(
            "expected => Array.from(document.querySelectorAll('#log-box > div'))"
            ".some(element => %s(element).indexOf(expected) !== -1)" % LINE_TEXT,
            arg=text, timeout=timeout * 1000)

    def wait_for_empty(self, timeout=5):
        self.page.wait_for_function(
            "document.getElementById('log-box').children.length === 0",
            timeout=timeout * 1000)

    def scrolled_to_bottom(self):
        return self.page.evaluate(
            "() => { const box = document.getElementById('log-box');"
            " return box.scrollTop + box.clientHeight >= box.scrollHeight - 2; }")


# ---------------------------------------------------------------------------
# Cases
# ---------------------------------------------------------------------------

def case_status_and_controls(ctx, ui, server):
    """The status dot, the status text and the button track the connection."""
    ctx.check(ui.status() == "未连接", "the page starts at 未连接")
    ctx.check(ui.state() == "idle", "the idle state dot is neutral")
    ctx.check(ui.page.inner_text("#toggleConnectBtn") == "启动", "the button offers 启动")
    ctx.check("connect-btn" in ui.toggle_class(), "启动 is the filled primary button")

    ui.connect()
    ctx.check(ui.state() == "ok", "the connected state dot is green")
    ctx.check(ui.page.inner_text("#toggleConnectBtn") == "停止", "the button becomes 停止")
    ctx.check("disconnect-btn" in ui.toggle_class(), "停止 is styled as the destructive action")
    ui.wait_for_text("连接成功")
    ctx.check(ui.page.evaluate(
        "() => Array.from(document.querySelectorAll('#log-box .line-info'))"
        ".some(line => line.textContent.indexOf('连接成功') !== -1)") is True,
        "the connection is reported as an info line")

    ui.disconnect()
    ctx.check(ui.state() == "idle", "the disconnected state dot is neutral again")
    ctx.check(ui.page.inner_text("#toggleConnectBtn") == "启动", "the button offers 启动 again")
    ctx.check("connect-btn" in ui.toggle_class(), "启动 is the primary button again")

    # onclose and the queued flush both land a moment after the click
    time.sleep(0.4)
    before = ui.lines()
    time.sleep(4.0)
    ctx.check(ui.lines() == before, "a manual stop is never retried")
    ctx.check(ui.status() == "已断开", "a manual stop stays disconnected")


def case_rendering_and_escaping(ctx, ui, server):
    """Lines are rendered verbatim, and HTML in a line stays text."""
    ui.connect()

    server.send_log(b"RENDER-PLAIN\n")
    server.send_log(b"ERROR: disk full\n")
    server.send_log(b"<script>window.__pwned = 1</script>\n")
    server.send_log(b"<img src=x onerror=\"window.__pwned = 2\">\n")
    big_line = "BIG-" + "z" * 60000
    server.send_log(big_line.encode() + b"\n")
    ui.wait_for_line(big_line.strip(), timeout=20)

    classes = ui.page.evaluate(
        "() => { const lines = Array.from(document.querySelectorAll('#log-box > div'));"
        " const pick = text => { const line = lines.find(item => (%s)(item) === text);"
        " return line ? line.className : null; };"
        " return { plain: pick('RENDER-PLAIN'), error: pick('ERROR: disk full') }; }" % LINE_TEXT)
    ctx.check(classes["plain"] == "", "a plain line is rendered without a colour class")
    ctx.check(classes["error"] == "line-error",
              "a line containing ERROR is drawn in the error colour")

    ctx.check(ui.page.evaluate("window.__pwned === undefined") is True,
              "a script tag in a log line does not run")
    ctx.check(ui.page.evaluate(
        "document.querySelectorAll('#log-box script, #log-box img').length") == 0,
        "a log line never becomes a DOM script or image element")
    ctx.check(ui.page.evaluate(
        "() => Array.from(document.querySelectorAll('#log-box > div'))"
        ".some(line => (%s)(line) === '<script>window.__pwned = 1</script>')" % LINE_TEXT) is True,
        "the script tag is shown as the text it was")

    shown = ui.page.evaluate(
        "() => { const line = Array.from(document.querySelectorAll('#log-box > div'))"
        ".find(element => element.textContent.indexOf('BIG-') === 0);"
        " return line ? (%s)(line).length : 0; }" % LINE_TEXT)
    ctx.check(shown == len(big_line.strip()), "a 60 KB line arrives whole (%d characters)" % shown)

    lines = ui.lines()
    ctx.check(lines.index("RENDER-PLAIN") < lines.index("ERROR: disk full"),
              "lines keep the order they were sent in")


def case_clear(ctx, ui, server):
    """清空 empties the box, drops the pending flush, and does not stop the stream."""
    ui.connect()
    server.send_lines("CLEAR", 40)
    ui.wait_for_line("CLEAR-00039")

    ui.page.click("#clearLogBtn")
    ctx.check(ui.line_count() == 0, "清空 removes every rendered line")
    time.sleep(0.4)
    ctx.check(ui.line_count() == 0, "no queued flush refills the box after 清空")

    # appendLog() with a flush 50 ms away, cleared in the same tick
    ui.page.evaluate("() => { appendLog('LATE-FLUSH'); clearLogBox(); }")
    time.sleep(0.4)
    ctx.check(ui.line_count() == 0, "清空 cancels the flush that is still pending")

    server.send_log(b"CLEAR-AFTER\n")
    ui.wait_for_line("CLEAR-AFTER")
    ctx.check(ui.line_count() == 1, "the stream keeps rendering after 清空")


def case_trim(ctx, ui, server):
    """The box keeps at most 10000 lines and drops the oldest ones in batches."""
    ui.connect()
    total = 12000
    server.send_lines("TRIM", total)
    ui.wait_for_line("TRIM-%05d" % (total - 1), timeout=60)
    time.sleep(0.5)

    count = ui.line_count()
    ctx.check(9000 <= count <= 10000,
              "after %d lines the box holds %d, inside the 10000 line cap" % (total, count))

    lines = ui.lines()
    ctx.check(lines[-1] == "TRIM-%05d" % (total - 1), "the newest line is the last one rendered")
    ctx.check("TRIM-00000" not in lines, "the oldest lines were trimmed away")
    ctx.check(all(line.startswith("TRIM-") for line in lines),
              "the internal connection lines were trimmed before the log lines")


def case_auto_scroll(ctx, ui, server):
    """Auto-scroll follows the newest line only while it is on."""
    ui.connect()
    server.send_lines("SCROLL", 200)
    ui.wait_for_line("SCROLL-00199")
    time.sleep(0.4)
    ctx.check(ui.scrolled_to_bottom(), "the view sticks to the newest line")

    ui.page.click("#toggleScrollBtn")
    ctx.check("scroll-off" in ui.page.get_attribute("#toggleScrollBtn", "class"),
              "turning auto-scroll off marks the button as off")
    ctx.check(ui.page.inner_text("#toggleScrollBtn") == "自动滚动",
              "the button keeps its text when it is off")

    ui.page.evaluate("document.getElementById('log-box').scrollTop = 0")
    server.send_lines("SCROLL-B", 50)
    ui.wait_for_line("SCROLL-B-00049")
    time.sleep(0.4)
    ctx.check(ui.page.evaluate("document.getElementById('log-box').scrollTop") == 0,
              "with auto-scroll off the view stays where the user left it")

    ui.page.click("#toggleScrollBtn")
    ctx.check("scroll-on" in ui.page.get_attribute("#toggleScrollBtn", "class"),
              "turning auto-scroll back on marks the button as on")
    server.send_log(b"SCROLL-C\n")
    ui.wait_for_line("SCROLL-C")
    time.sleep(0.4)
    ctx.check(ui.scrolled_to_bottom(), "turning it back on resumes following")


def case_save(ctx, ui, server):
    """保存 exports the rendered lines under a timestamped name."""
    ui.connect()
    server.send_log(b"SAVE-ONE\n")
    server.send_log(b"SAVE-TWO\n")
    ui.wait_for_line("SAVE-TWO")
    time.sleep(0.2)

    with ui.page.expect_download(timeout=10000) as download_info:
        ui.page.click("#saveLogBtn")
    download = download_info.value
    name = download.suggested_filename
    ctx.check(re.match(r"^log_\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}\.txt$", name) is not None,
              "the download is named log_<date>_<time>.txt (%s)" % name)

    path = os.path.join(server.workdir, "saved.txt")
    download.save_as(path)
    with open(path, "r", encoding="utf-8", errors="replace") as stream:
        content = stream.read()
    ctx.check("SAVE-ONE" in content and "SAVE-TWO" in content,
              "the saved file holds the rendered lines")
    ctx.check(content.index("SAVE-ONE") < content.index("SAVE-TWO"),
              "the saved lines keep their order")

    # A line appended in the same tick is flushed before the export
    with ui.page.expect_download(timeout=10000) as download_info:
        ui.page.evaluate("() => { appendLog('SAVE-PENDING'); saveLog(); }")
    path = os.path.join(server.workdir, "saved_pending.txt")
    download_info.value.save_as(path)
    with open(path, "r", encoding="utf-8", errors="replace") as stream:
        content = stream.read()
    ctx.check("SAVE-PENDING" in content, "保存 flushes the lines that are still pending")


def case_about(ctx, ui, server):
    """关于 names the transport and the version the page was built with."""
    ctx.check(ui.page.is_visible("#aboutDialog") is False, "the about dialog starts closed")
    ui.page.click("#aboutBtn")
    ctx.check(ui.page.is_visible("#aboutDialog") is True, "关于 opens the dialog")

    mode = ui.page.inner_text("#aboutMode")
    ctx.check(mode == TRANSPORT_NAMES[ctx.web_type],
              "the dialog names the transport the page was built with (%s)" % mode)
    version = ui.page.evaluate("document.querySelectorAll('#aboutDialog dd')[0].textContent")
    ctx.check(re.match(r"^V\d+\.\d+\.\d+$", version.strip()) is not None,
              "the dialog shows the version (%s)" % version)

    ui.page.click("#aboutDialog button")
    ctx.check(ui.page.is_visible("#aboutDialog") is False, "关闭 closes the dialog")


def case_reconnect(ctx, ui, server):
    """A dead server is retried three times; after that the page stops trying."""
    ui.connect()
    server.kill()
    ui.wait_status("重连失败", timeout=30)
    ui.wait_for_text("重连失败，停止尝试")

    lines = ui.lines()
    attempts = [line for line in lines if "秒后尝试重连" in line]
    ctx.check(1 <= len(attempts) <= 3,
              "the page retries at most three times (%d)" % len(attempts))
    ctx.check(any("重连失败，停止尝试" in line for line in lines),
              "giving up is reported in the log")

    # The last failed attempt and its status update land after 重连失败 itself,
    # so wait for the page to go quiet before checking that it stays quiet.
    last_count = ui.line_count()
    stable = 0.0
    deadline = time.time() + 20
    while time.time() < deadline:
        time.sleep(0.5)
        count = ui.line_count()
        if count == last_count:
            stable += 0.5
            if stable >= 4.0:
                break
        else:
            stable = 0.0
            last_count = count

    ctx.check(ui.status() == "重连失败", "the status settles on 重连失败")
    before = ui.lines()
    time.sleep(4.0)
    ctx.check(ui.lines() == before,
              "no further connection attempts are made or reported after giving up")

    # 启动 has to start over rather than just close the connection that gave up
    ui.page.click("#clearLogBtn")
    ui.page.click("#toggleConnectBtn")
    ui.wait_for_text("正在连接", timeout=5)
    ctx.check(ui.status() != "已断开", "启动 starts over after the retries ran out")


def case_long_run(ctx, ui, server):
    """100000 lines leave the DOM and the JavaScript heap flat."""
    ui.connect()
    cdp = ui.page.context.new_cdp_session(ui.page)
    cdp.send("HeapProfiler.enable")

    def heap_bytes():
        cdp.send("HeapProfiler.collectGarbage")
        return cdp.send("Runtime.getHeapUsage")["usedSize"]

    server.send_lines("LONG", 20000)
    ui.wait_for_line("LONG-19999", timeout=60)
    time.sleep(1.0)
    early = heap_bytes()

    server.send_lines("LONG", 80000, start=20000)
    ui.wait_for_line("LONG-99999", timeout=120)
    time.sleep(1.0)
    late = heap_bytes()

    count = ui.line_count()
    ctx.check(count <= 10000, "100000 lines leave at most 10000 in the box (%d)" % count)
    ctx.check("LONG-99999" in ui.lines(), "the newest line is still rendered")
    growth = late - early
    ctx.check(growth < 12 * 1024 * 1024,
              "the page heap stays flat from 20000 to 100000 lines "
              "(%+d bytes: %d -> %d)" % (growth, early, late))

    started = time.time()
    ui.page.click("#clearLogBtn")
    ui.wait_for_empty()
    ctx.check(time.time() - started < 2.0,
              "the page still answers a click within 2s after 100000 lines")


CASES = [
    ("status and controls", case_status_and_controls),
    ("rendering and escaping", case_rendering_and_escaping),
    ("clear", case_clear),
    ("trimming", case_trim),
    ("auto scroll", case_auto_scroll),
    ("save", case_save),
    ("about", case_about),
    ("reconnect", case_reconnect),
    ("long run", case_long_run),
]


# ---------------------------------------------------------------------------
# Runner
# ---------------------------------------------------------------------------

class Context(object):
    def __init__(self, binary, web_type, browser):
        self.binary = binary
        self.web_type = web_type
        self.browser = browser
        self.failures = []

    def check(self, condition, message):
        if condition:
            print("PASS %s" % message)
        else:
            print("FAIL %s" % message)
            self.failures.append(message)


def run_case(ctx, label, case):
    print("-- %s" % label)
    server = Server(ctx.binary, ctx.web_type)
    context = ctx.browser.new_context(accept_downloads=True)
    failures_before = len(ctx.failures)
    try:
        ui = Ui(context, server.web_port)
        case(ctx, ui, server)
        ctx.check(ui.errors == [], "the page raised no uncaught exception %s" % ui.errors)
    finally:
        if len(ctx.failures) > failures_before:
            print("server output:")
            print(server.output())
        context.close()
        server.stop()


def main():
    if len(sys.argv) < 3:
        print("usage: web_ui_test.py <LogServer> <SSE|WEBSOCKET>")
        return 2

    binary = os.path.abspath(sys.argv[1])
    web_type = sys.argv[2].upper()
    if web_type not in TRANSPORT_NAMES:
        print("unknown web server type: %s" % web_type)
        return 2

    try:
        playwright = sync_playwright().start()
    except PlaywrightError as error:
        print("playwright could not start: %s" % error)
        return 2

    try:
        browser = playwright.chromium.launch()
    except PlaywrightError as error:
        playwright.stop()
        print("Chromium for Playwright is not installed (python -m playwright install chromium): %s" % error)
        return 2

    ctx = Context(binary, web_type, browser)
    print("running %s UI tests" % web_type)
    try:
        for label, case in CASES:
            run_case(ctx, label, case)
    finally:
        browser.close()
        playwright.stop()

    if ctx.failures:
        print("%d check(s) failed" % len(ctx.failures))
        return 1

    print("all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
