#!/usr/bin/env bash

set -e          # exit on error
set -u          # exit on unset variable
set -o pipefail # exit on pipe error

function kill_server() {
    if [ -n "${server_pid:-}" ] && [ "$(ps -p "$server_pid" | wc -l)" -eq 2 ]; then
        echo
        echo "Stopping server..."
        kill "$server_pid"
    fi
}

trap kill_server EXIT

PORT=8080
CURL_CERTS=("--cacert" "main/certs/cert.pem" "--cert" "main/certs/client.crt" "--key" "main/certs/client.key")
ESP_IP=192.168.1.32
local_ip=$(ip -4 addr show wlp3s0 | grep -oP '(?<=inet\s)\d+(\.\d+){3}')

echo "Building..."
set +e
build_output=$(idf.py build 2>&1)
build_status=$?
set -e
if [ $build_status -ne 0 ]; then
    echo
    echo "Build failed:"
    echo
    echo "$build_output"
    exit $build_status
fi

echo
echo "Starting server..."
python update-server.py "$local_ip" "$PORT" &
sleep 1
server_pid=$!

echo
echo -n "Sending OTA request... "
curl "${CURL_CERTS[@]}" -X POST https://${ESP_IP}/ota -H "payload_url: https://${local_ip}:${PORT}/dwc_keeper.bin"
echo

echo
payload_sha256=$(sha256sum build/dwc_keeper.elf | awk '{print $1}')
while true; do
    echo -n "Checking for image update... "
    status_code=$(
        curl -s "${CURL_CERTS[@]}" https://${ESP_IP}/ota/check -w "%{http_code}" -o /dev/null -H "sha256: $payload_sha256"
    )
    if [ "$status_code" -eq 200 ]; then
        echo "Image updated!"
        echo
        echo "Update successful!"
        exit 0
    elif [ "$status_code" -eq 204 ]; then
        echo "Image is not up to date"
        sleep 3
    else
        echo "Error: $status_code"
        exit 1
    fi
done
