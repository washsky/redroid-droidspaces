#!/usr/bin/env python3
"""Test actual transport auth with fragmented replies and rejected credentials."""
import base64
from pathlib import Path
import socket
import subprocess
import tempfile
import threading
ROOT=Path(__file__).resolve().parents[1]
def exact(s,n):
    b=b""
    while len(b)<n:
        x=s.recv(n-len(b));assert x;b+=x
    return b
with tempfile.TemporaryDirectory(prefix="auth-test-") as t:
    t=Path(t)
    (t/"probe.c").write_text('#include "'+str(ROOT/"src/netmanager/proxy-common.h")+'''"
int main(int n,char **v){
 if(n!=3)return 2;struct px_config c;px_defaults(&c);c.auth=1;strcpy(c.username,"user name");strcpy(c.password,"p@ss : with spaces");strcpy(c.protocol,v[1]);strcpy(c.host,"127.0.0.1");c.port=atoi(v[2]);
 struct sockaddr_in d={.sin_family=AF_INET,.sin_port=htons(443)};inet_pton(AF_INET,"1.1.1.1",&d.sin_addr);int fd=px_tunnel(&c,&d);if(fd<0)return 3;
 char b[2]="OK";int result=px_io(fd,b,2,1,px_now()+3000);close(fd);return result?4:0;
}
''')
    subprocess.run(["cc","-O2","-Wall","-Wextra","-Werror","-Wno-misleading-indentation",str(t/"probe.c"),"-o",str(t/"probe")],check=True)
    for proto in ("socks5","http"):
        for reject in (False,True):
            listener=socket.socket();listener.bind(("127.0.0.1",0));listener.listen();errors=[]
            def serve():
                try:
                    s,_=listener.accept();s.settimeout(5)
                    with s:
                        if proto=="socks5":
                            assert exact(s,3)==bytes.fromhex("050102");s.sendall(bytes.fromhex("0502"))
                            assert exact(s,1)==b"\x01";u=exact(s,exact(s,1)[0]);p=exact(s,exact(s,1)[0])
                            assert u==b"user name" and p==b"p@ss : with spaces"
                            s.sendall(b"\x01\x01" if reject else b"\x01\x00")
                            if reject:return
                            assert exact(s,10)==bytes.fromhex("050100010101010101bb")
                            for x in bytes.fromhex("05000001000000000000"):s.sendall(bytes([x]))
                        else:
                            request=b""
                            while not request.endswith(b"\r\n\r\n"):request+=exact(s,1)
                            assert b"Proxy-Authorization: Basic "+base64.b64encode(b"user name:p@ss : with spaces")+b"\r\n" in request
                            s.sendall(b"HTTP/1.1 407 Authentication Required\r\n\r\n" if reject else b"HTTP/1.1 200 OK\r\n\r\n")
                            if reject:return
                        assert exact(s,2)==b"OK"
                except Exception as e:errors.append(repr(e))
            thread=threading.Thread(target=serve,daemon=True);thread.start()
            p=subprocess.run([str(t/"probe"),proto,str(listener.getsockname()[1])],timeout=10);thread.join(6);listener.close()
            assert not errors,errors
            assert p.returncode==(3 if reject else 0),(proto,reject,p.returncode)
            print("PASS",proto,"auth rejected" if reject else "authenticated tunnel")
