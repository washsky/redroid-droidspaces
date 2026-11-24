#!/usr/bin/env python3
"""Exercise the real C handshake/relay against fragmented local mock peers."""
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[1]

def exact(sock, size):
    data = b""
    while len(data) < size:
        part = sock.recv(size - len(data))
        assert part, "unexpected EOF"
        data += part
    return data

with tempfile.TemporaryDirectory(prefix="proxy-test-") as temp:
    temp = Path(temp)
    harness = temp / "harness.c"
    harness.write_text('''#define main proxy_daemon_main
#include "''' + str(ROOT / "src/netmanager/proxy-relay.c") + '''"
#undef main
int main(int n,char **v){
 if(n==4&&!strcmp(v[1],"relay")){relay(atoi(v[2]),atoi(v[3]));return 0;}
 if(n!=4)return 2;
 struct px_config c;px_defaults(&c);snprintf(c.protocol,sizeof(c.protocol),"%s",v[1]);c.port=atoi(v[2]);strcpy(c.host,"127.0.0.1");
 struct sockaddr_in d={.sin_family=AF_INET,.sin_port=htons(443)};inet_pton(AF_INET,"1.1.1.1",&d.sin_addr);
 int fd=px_tunnel(&c,&d);if(fd<0)return 3;
 char b[5]="hello";int64_t end=px_now()+3000;
 if(px_io(fd,b,5,1,end)||px_io(fd,b,5,0,end))return 4;
 return memcmp(b,"HELLO",5)?5:0;
}
''')
    executable = temp / "harness"
    subprocess.run(["cc", "-O2", "-Wall", "-Wextra", "-Werror", "-pthread",
                    str(harness), "-o", str(executable)], check=True)
    for protocol, reject in [("socks5", False), ("http", False),
                             ("socks5", True), ("http", True)]:
        listener = socket.socket()
        listener.bind(("127.0.0.1", 0))
        listener.listen()
        port = listener.getsockname()[1]
        errors = []

        def serve():
            try:
                connection, _ = listener.accept()
                connection.settimeout(4)
                with connection:
                    if protocol == "socks5":
                        assert exact(connection, 3) == b"\x05\x01\x00"
                        connection.sendall(b"\x05\x02" if reject else b"\x05\x00")
                        if reject:
                            return
                        assert exact(connection, 10) == bytes.fromhex("050100010101010101bb")
                        for byte in b"\x05\x00\x00\x03\x03abc\x00\x50":
                            connection.sendall(bytes([byte]))
                    else:
                        request = b""
                        while not request.endswith(b"\r\n\r\n"):
                            request += exact(connection, 1)
                        assert request.startswith(b"CONNECT 1.1.1.1:443 HTTP/1.1")
                        connection.sendall(b"HTTP/1.1 403 Forbidden\r\n\r\n" if reject else
                                           b"HTTP/1.1 200 Connection established\r\nX-Test: 1\r\n\r\n")
                        if reject:
                            return
                    assert exact(connection, 5) == b"hello"
                    connection.sendall(b"HELLO")
            except Exception as error:
                errors.append(repr(error))

        thread = threading.Thread(target=serve, daemon=True)
        thread.start()
        result = subprocess.run([str(executable), protocol, str(port), "echo"], timeout=10)
        thread.join(5)
        listener.close()
        assert not errors, errors
        assert result.returncode == (3 if reject else 0), (protocol, reject, result.returncode)
        print("PASS", protocol, "rejection" if reject else "fragmented handshake and echo")

    left, inner_left = socket.socketpair()
    inner_right, right = socket.socketpair()
    inner_left.setblocking(False)
    inner_right.setblocking(False)
    child = subprocess.Popen([str(executable), "relay", str(inner_left.fileno()), str(inner_right.fileno())],
                             pass_fds=(inner_left.fileno(), inner_right.fileno()))
    inner_left.close()
    inner_right.close()
    payload = [os.urandom(2 * 1024 * 1024), os.urandom(2 * 1024 * 1024)]
    received = [bytearray(), bytearray()]
    errors = []

    def send(sock, data):
        try:
            sock.sendall(data)
            sock.shutdown(socket.SHUT_WR)
        except Exception as error:
            errors.append(repr(error))

    def receive(sock, output):
        try:
            while True:
                data = sock.recv(8192)
                if not data:
                    break
                output.extend(data)
        except Exception as error:
            errors.append(repr(error))

    for sock in (left, right):
        sock.settimeout(12)
    threads = [threading.Thread(target=send, args=(left, payload[0]), daemon=True),
               threading.Thread(target=send, args=(right, payload[1]), daemon=True),
               threading.Thread(target=receive, args=(left, received[0]), daemon=True),
               threading.Thread(target=receive, args=(right, received[1]), daemon=True)]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join(15)
    assert not errors, errors
    assert bytes(received[0]) == payload[1] and bytes(received[1]) == payload[0]
    assert child.wait(timeout=3) == 0
    left.close()
    right.close()
    print("PASS bidirectional 2 MiB, backpressure and half-close")
