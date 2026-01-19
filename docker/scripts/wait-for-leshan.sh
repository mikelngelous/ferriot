#!/bin/bash
TIMEOUT=${1:-60}
LESHAN_HOST="${LESHAN_HOST:-localhost}"
LESHAN_API_PORT="${LESHAN_API_PORT:-8080}"

echo "Waiting for Leshan (max ${TIMEOUT}s)..."

for i in $(seq 1 $TIMEOUT); do
    if curl -sf "http://${LESHAN_HOST}:${LESHAN_API_PORT}/api/clients" > /dev/null 2>&1; then
        echo "Leshan is ready! (${i}s)"
        exit 0
    fi
    sleep 1
    echo -n "."
done

echo ""
echo "ERROR: Leshan did not start within ${TIMEOUT} seconds"
docker compose logs leshan-server | tail -50
exit 1
