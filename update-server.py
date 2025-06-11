#!/usr/bin/env python3

import http.server
import ssl
import sys


class BuildFilesHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory="build", **kwargs)


if __name__ == "__main__":
    host = sys.argv[1]
    port = int(sys.argv[2])

    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(
        certfile="main/certs/client.crt", keyfile="main/certs/client.key"
    )

    httpd = http.server.ThreadingHTTPServer((host, port), BuildFilesHandler)
    httpd.socket = context.wrap_socket(httpd.socket, server_side=True)
    httpd.serve_forever()
