"""Drives a running editor or game's Lua debugger the way VS Code does, for checking it by hand.

    python dap_check.py <script.lua> <line> [--port 4711]

Start the editor (and press Play) or Game.exe with lua_debugger.enabled in engine_settings.yaml
first. The client attaches, sets a breakpoint at <line> of <script.lua>, waits for the stop, prints
the stack and the top frame's locals, steps over once, continues and disconnects. It exits nonzero,
saying why, when any step fails.
"""

import argparse
import json
import os
import socket
import sys
import time


class Client:
    def __init__(self, port):
        deadline = time.time() + 20
        while True:
            try:
                self.sock = socket.create_connection(("127.0.0.1", port), timeout=10)
                break
            except OSError:
                if time.time() > deadline:
                    sys.exit(f"no debugger listening on 127.0.0.1:{port}")
                time.sleep(0.25)
        self.seq = 0
        self.buffer = b""
        self.unread = []  # messages that arrived while another was awaited

    def send(self, command, arguments=None):
        self.seq += 1
        body = json.dumps({"seq": self.seq, "type": "request", "command": command, "arguments": arguments or {}}).encode()
        self.sock.sendall(b"Content-Length: %d\r\n\r\n" % len(body) + body)
        return self.seq

    def receive(self):
        data = self.sock.recv(65536)
        if not data:
            sys.exit("the debugger closed the connection")
        self.buffer += data

    def read(self):
        while b"\r\n\r\n" not in self.buffer:
            self.receive()
        header, self.buffer = self.buffer.split(b"\r\n\r\n", 1)
        length = int(header.decode().split("Content-Length:")[1].strip())
        while len(self.buffer) < length:
            self.receive()
        body, self.buffer = self.buffer[:length], self.buffer[length:]
        return json.loads(body)

    def wait(self, predicate, what):
        for message in self.unread:
            if predicate(message):
                self.unread.remove(message)
                return message
        while True:
            message = self.read()
            if message.get("event") == "output":
                continue
            if predicate(message):
                return message
            self.unread.append(message)

    def request(self, command, arguments=None):
        seq = self.send(command, arguments)
        response = self.wait(lambda m: m.get("type") == "response" and m.get("request_seq") == seq, command)
        if not response["success"]:
            sys.exit(f"{command} failed: {response.get('message')}")
        return response.get("body", {})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("script")
    parser.add_argument("line", type=int)
    parser.add_argument("--port", type=int, default=4711)
    args = parser.parse_args()

    client = Client(args.port)
    capabilities = client.request("initialize", {"adapterID": "hedgehog-lua", "linesStartAt1": True})
    client.wait(lambda m: m.get("event") == "initialized", "initialized")
    placed = client.request("setBreakpoints", {"source": {"path": os.path.abspath(args.script)},
                                               "breakpoints": [{"line": args.line}]})["breakpoints"][0]
    print(f"capabilities: {sorted(k for k, v in capabilities.items() if v)}")
    print(f"breakpoint: line {placed['line']}, verified {placed['verified']}")
    client.request("configurationDone")
    client.request("attach", {"port": args.port})

    stop = client.wait(lambda m: m.get("event") == "stopped", "stopped")
    print(f"stopped: {stop['body']['reason']}")
    frames = client.request("stackTrace", {"threadId": 1})["stackFrames"]
    for frame in frames:
        print(f"  #{frame['id']} {frame['name']} {frame.get('source', {}).get('name', '[C]')}:{frame['line']}")
    scopes = client.request("scopes", {"frameId": frames[0]["id"]})["scopes"]
    for variable in client.request("variables", {"variablesReference": scopes[0]["variablesReference"]})["variables"]:
        print(f"  local {variable['name']} = {variable['value']}")

    client.request("next", {"threadId": 1})
    step = client.wait(lambda m: m.get("event") == "stopped", "stopped after next")
    line = client.request("stackTrace", {"threadId": 1})["stackFrames"][0]["line"]
    print(f"stepped ({step['body']['reason']}) to line {line}")

    client.request("setBreakpoints", {"source": {"path": os.path.abspath(args.script)}, "breakpoints": []})
    client.request("continue", {"threadId": 1})
    client.request("disconnect")
    print("ok")


if __name__ == "__main__":
    main()
