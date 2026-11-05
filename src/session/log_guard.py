"""Writes the shell's messages into the session's log, keeping it small, and
starts a shell again that has got stuck saying the same thing.

    keep-running run-shell 2>&1 | python log_guard.py LOG SHELL_PID_FILE

A line the same as the one before is not written again; once a different
one comes, or a while has passed, one line says how often it came. The log
lives in memory ($XDG_RUNTIME_DIR), so a shell stuck in a loop that says the
same thing thousands of times a second would otherwise fill it, as one did
with "QSocketNotifier: Invalid socket" (a race at its start, 2026-11-05).

FLOOD_LINES lines within FLOOD_SECONDS, the same or not, mean the shell is
stuck in a loop: it is ended (TERM, then KILL), and keep-running starts it
again as after a crash. A shell at work never says this much.

Nothing may end this program early: the shell writes into the pipe it reads,
and a shell whose reader is gone ends at its next line.
"""

import os
import signal
import sys
import time

FLOOD_LINES = 5000
FLOOD_SECONDS = 5
# How long a repeated line may go unreported.
REPORT_EVERY_SECONDS = 60
# How long a stuck shell has to end after TERM.
END_WAIT_SECONDS = 3


def shell_pid(pid_file):
    """The shell's process ID, if it is still the shell (Quickshell's `qs`)."""
    try:
        pid = int(open(pid_file).read().strip())
        if open(f"/proc/{pid}/comm").read().strip() == "qs":
            return pid
    except (OSError, ValueError):
        pass
    return None


class Guard:
    def __init__(self, log, pid_file):
        self.log = log
        self.pid_file = pid_file
        self.last = None
        self.repeats = 0
        self.reported_at = time.monotonic()
        self.flood_started = 0.0
        self.flood_count = 0

    def report(self):
        if self.repeats:
            self.log.write(
                f"log_guard: the line above came {self.repeats} more times\n".encode()
            )
        self.repeats = 0
        self.reported_at = time.monotonic()

    def take(self, line):
        now = time.monotonic()
        if line != self.last:
            self.report()
            self.log.write(line)
            self.last = line
        else:
            self.repeats += 1
            if now - self.reported_at >= REPORT_EVERY_SECONDS:
                self.report()

        if self.flood_count == 0 or now - self.flood_started > FLOOD_SECONDS:
            self.flood_started, self.flood_count = now, 0
        self.flood_count += 1
        if self.flood_count >= FLOOD_LINES:
            self.report()
            self.end_stuck_shell()
            self.flood_count = 0

    def end_stuck_shell(self):
        pid = shell_pid(self.pid_file)
        if pid is None:
            return
        self.log.write(
            f"log_guard: the shell said {FLOOD_LINES} lines within {FLOOD_SECONDS} seconds "
            "and seems stuck; starting it again\n".encode()
        )
        os.kill(pid, signal.SIGTERM)
        for _ in range(END_WAIT_SECONDS * 10):
            time.sleep(0.1)
            if shell_pid(self.pid_file) != pid:
                return
        os.kill(pid, signal.SIGKILL)


def main():
    if len(sys.argv) != 3:
        sys.exit("log_guard: give the log file and the shell's pid file")
    with open(sys.argv[1], "ab", buffering=0) as log:
        guard = Guard(log, sys.argv[2])
        for line in sys.stdin.buffer:
            try:
                guard.take(line)
            except Exception as error:  # noqa: BLE001 - keep reading whatever happens
                print(f"log_guard: {error}", file=sys.stderr)
        guard.report()


if __name__ == "__main__":
    main()
