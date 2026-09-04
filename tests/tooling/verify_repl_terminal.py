#!/usr/bin/env python3
"""Drive the rich REPL through a real pseudo-terminal."""

from __future__ import annotations

import os
import pathlib
import re
import select
import signal
import sys
import time


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    if os.name == "nt":
        print("Kyna rich-REPL pseudo-terminal test skipped on Windows")
        return 0

    import pty

    cli = pathlib.Path(sys.argv[1]).resolve()
    pid, terminal = pty.fork()
    if pid == 0:
        os.execv(str(cli), [str(cli), "repl"])

    captured = bytearray()

    def drain(duration: float) -> None:
        deadline = time.monotonic() + duration
        while time.monotonic() < deadline:
            readable, _, _ = select.select([terminal], [], [], 0.05)
            if not readable:
                continue
            try:
                chunk = os.read(terminal, 65536)
            except OSError:
                return
            if not chunk:
                return
            captured.extend(chunk)

    def send(data: bytes, settle: float = 0.55) -> None:
        # Each accepted line tears down one FTXUI app and starts the next. Give
        # the new raw-terminal reader time to take ownership before typing.
        time.sleep(settle)
        os.write(terminal, data)
        drain(0.9)

    def wait_exited(timeout: float):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            drain(0.1)
            completed, candidate = os.waitpid(pid, os.WNOHANG)
            if completed == pid:
                return candidate
        return None

    try:
        drain(0.8)
        send(b"let total: int = 40\r")
        send(b"total = total + 2\r")
        send(b"print(totl)", settle=0.4)
        send(b"\x1b[D\x1b[D", settle=0.2)  # Move before the final 'l'.
        send(b"a\r", settle=0.2)           # Correct totl -> total and execute.
        send(b"\x1b[A\r")       # Recall the corrected command and execute again.
        send(
            b"set pasted_fixed = 10;\n"
            b"let pasted_changing: int = 10;\n"
            b"pasted_changing = pasted_changing + 5;\n"
            b"# Pasted comments must not be joined to the previous statement.\n"
            b'print("pasted block", pasted_fixed, pasted_changing);\n'
        )
        send(b":pro\t\r")       # Complete :project and print workspace context.
        # macOS CI often delivers Ctrl-D while FTXUI is still leaving cooked
        # mode / querying the terminal, which turns EOT into stdin EOF instead
        # of Event::CtrlD. Wait for the next prompt, then retry.
        drain(1.5)
        status = None
        for _ in range(5):
            os.write(terminal, b"\x04")
            status = wait_exited(0.7)
            if status is not None:
                break
        if status is None:
            os.write(terminal, b":quit\r")
            status = wait_exited(3)
        if status is None:
            os.kill(pid, signal.SIGTERM)
            os.waitpid(pid, 0)
            preview = captured.decode("utf-8", errors="replace")[-3000:]
            raise AssertionError(f"rich REPL did not exit after Ctrl-D:\n{preview}")
    finally:
        os.close(terminal)

    rendered = captured.decode("utf-8", errors="replace")
    rendered = re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", rendered)
    require("interactive playground" in rendered, "rich REPL banner was not rendered")
    require(len(re.findall(r"(?<!\d)42(?!\d)", rendered)) >= 2,
            f"cursor editing or history recall failed:\n{rendered[-2000:]}")
    require("undefined name" not in rendered, f"edited command was not applied:\n{rendered[-2000:]}")
    require("pasted block 10 15" in rendered,
            f"multi-line paste was not preserved as one submission:\n{rendered[-3000:]}")
    require("expected expression" not in rendered,
            f"multi-line paste joined or fragmented source lines:\n{rendered[-3000:]}")
    require("Project context" in rendered and "Status:    not initialized" in rendered,
            f"project command or workspace header was not rendered:\n{rendered[-3000:]}")
    print("ky rich-REPL terminal verification passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
