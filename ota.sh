#!/usr/bin/env bash

set -e          # exit on error
set -u          # exit on unset variable
set -o pipefail # exit on pipe error

function kill_server() {
    # shellcheck disable=SC2317
    if [ -n "${server_pid:-}" ] && [ "$(ps -p "$server_pid" | wc -l)" -eq 2 ]; then
        echo
        echo "Stopping server..."
        kill "$server_pid"
    fi
}

trap kill_server EXIT

PORT=8080
CURL_CERTS=("--cert" "main/certs/client.pem" "--key" "main/certs/client.key")
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
sleep 2
server_pid=$!

echo
echo -n "Sending OTA request... "
curl "${CURL_CERTS[@]}" -X POST --fail https://${ESP_IP}/ota
echo
sleep 2
echo

echo
payload_sha256=$(sha256sum build/dwc_keeper.elf | awk '{print $1}')
echo "Checking for image update... "
curl "${CURL_CERTS[@]}" https://${ESP_IP}/ota/check -H "sha256: $payload_sha256" --retry 10 --retry-delay 5 --retry-all-errors --fail --max-time 5
echo

echo
echo "Update successful! Tailing logs..."
# curl -N "${CURL_CERTS[@]}" https://${ESP_IP}/logs
