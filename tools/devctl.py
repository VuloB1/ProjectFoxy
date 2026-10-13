"""Talk to a Project Foxy built with -DPROJECTFOXY_DEV_SERVER=ON and started with PROJECTFOXY_DEV_PORT set.

    python tools/devctl.py [--port 47800] items
    python tools/devctl.py get maximizeButton width
    python tools/devctl.py click gifButton
    python tools/devctl.py grab C:/temp/window.png
"""
import argparse
import socket

parser = argparse.ArgumentParser()
parser.add_argument("--port", type=int, default=47800)
parser.add_argument("command", nargs="+")
args = parser.parse_args()

with socket.create_connection(("127.0.0.1", args.port), timeout=10) as s:
    s.sendall((" ".join(args.command) + "\n").encode())
    data = b""
    while not data.endswith(b"\n.\n"):
        chunk = s.recv(65536)
        if not chunk:
            break
        data += chunk
print(data.decode("utf-8", "replace").removesuffix("\n.\n"))
