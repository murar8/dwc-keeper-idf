#!/bin/bash

set -e          # exit on error
set -u          # exit on undefined variable
set -o pipefail # exit on pipe error

rm -rf main/certs/*
mkcert -cert-file main/certs/server.pem -key-file main/certs/server.key esp32 192.168.1.32
mkcert -cert-file main/certs/client.pem -key-file main/certs/client.key -client thinkpad localhost 192.168.1.150
# generate p12 file for client -- no password
openssl pkcs12 -export -out main/certs/client.p12 -in main/certs/client.pem -inkey main/certs/client.key -passout pass:
cp "$HOME"/.local/share/mkcert/rootCA.pem main/certs/ca.pem
