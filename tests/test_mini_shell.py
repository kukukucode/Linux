#!/usr/bin/env python3

import os
import pty
import select
import signal
import sys
import time


SHELL = "./build/mini_shell"
PROMPT = b"mini$ "


def read_until(fd, marker, timeout=5):
    data = b""
    deadline = time.monotonic() + timeout

    while marker not in data:
        remaining = deadline - time.monotonic()

        if remaining <= 0:
            raise RuntimeError(
                "timeout waiting for {!r}\noutput:\n{}".format(
                    marker,
                    data.decode(errors="replace"),
                )
            )

        readable, _, _ = select.select(
            [fd],
            [],
            [],
            remaining,
        )

        if not readable:
            continue

        chunk = os.read(fd, 4096)

        if not chunk:
            break

        data += chunk

    return data.decode(errors="replace").replace("\r\n", "\n")


def send(fd, command):
    os.write(fd, command.encode() + b"\n")


def require(text, expected, test_name):
    if expected not in text:
        raise RuntimeError(
            "{} failed\nexpected: {!r}\noutput:\n{}".format(
                test_name,
                expected,
                text,
            )
        )


def main():
    pid, fd = pty.fork()

    if pid == 0:
        os.execv(
            SHELL,
            [SHELL],
        )

    try:
        read_until(fd, PROMPT)

        # Basic external command
        send(fd, "echo hello")
        output = read_until(fd, PROMPT)
        require(
            output,
            "\nhello\n",
            "external command",
        )

        # Pipeline
        send(fd, "printf hello | wc -c")
        output = read_until(fd, PROMPT)
        require(
            output,
            "\n5\n",
            "pipeline",
        )

        # cd builtin
        send(fd, "cd /")
        read_until(fd, PROMPT)

        send(fd, "pwd")
        output = read_until(fd, PROMPT)
        require(
            output,
            "\n/\n",
            "cd builtin",
        )

        # Output + input redirection
        temp_path = "/tmp/mini_shell_test_{}.txt".format(pid)

        send(
            fd,
            "echo redirected > {}".format(temp_path),
        )
        read_until(fd, PROMPT)

        send(
            fd,
            "cat < {}".format(temp_path),
        )
        output = read_until(fd, PROMPT)
        require(
            output,
            "\nredirected\n",
            "redirection",
        )

        try:
            os.unlink(temp_path)
        except FileNotFoundError:
            pass

        send(fd, "exit")

        _, status = os.waitpid(pid, 0)

        if not os.WIFEXITED(status):
            raise RuntimeError(
                "mini shell did not exit normally"
            )

        if os.WEXITSTATUS(status) != 0:
            raise RuntimeError(
                "mini shell exited with status {}".format(
                    os.WEXITSTATUS(status)
                )
            )

        print("mini shell smoke tests passed")

    except Exception:
        try:
            os.kill(pid, signal.SIGKILL)
        except ProcessLookupError:
            pass

        try:
            os.waitpid(pid, 0)
        except ChildProcessError:
            pass

        raise

    finally:
        os.close(fd)


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(
            "mini shell test failed:",
            exc,
            file=sys.stderr,
        )
        sys.exit(1)
