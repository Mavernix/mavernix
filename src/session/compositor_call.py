#!/usr/bin/env python3
"""Asks the running compositor to do one thing, through its control socket.

    compositor_call.py METHOD

for example `scale/toggle`; used by touchpad gestures. Wayfire's ipc plugin
listens on WAYFIRE_SOCKET; a message is a 4-byte little-endian length and
that many bytes of JSON.
"""

import json
import os
import socket
import struct
import sys

LENGTH_FORMAT = "<I"


def fail(message):
    sys.exit(f"compositor_call: {message}")


def main():
    if len(sys.argv) != 2:
        fail("give one method, for example scale/toggle")
    path = os.environ.get("WAYFIRE_SOCKET")
    if not path:
        fail("WAYFIRE_SOCKET is not set; this is meant to be run inside the desktop session")

    body = json.dumps({"method": sys.argv[1], "data": {}}).encode()
    with socket.socket(socket.AF_UNIX) as connection:
        connection.connect(path)
        connection.sendall(struct.pack(LENGTH_FORMAT, len(body)) + body)

        # Read the whole answer so the compositor never writes to a closed socket.
        (length,) = struct.unpack(LENGTH_FORMAT, connection.recv(4, socket.MSG_WAITALL))
        answer = json.loads(connection.recv(length, socket.MSG_WAITALL))
    if "error" in answer:
        fail(f'the compositor answered "{answer["error"]}" to {sys.argv[1]}')


if __name__ == "__main__":
    main()
