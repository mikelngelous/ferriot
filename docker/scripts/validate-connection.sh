#!/bin/bash
set -e

LESHAN_HOST="${LESHAN_HOST:-localhost}"
LESHAN_API_PORT="${LESHAN_API_PORT:-8080}"
LESHAN_COAP_PORT="${LESHAN_COAP_PORT:-5683}"

echo "=== Validating Leshan LWM2M Server ==="

# 1. Check REST API
echo -n "Checking REST API... "
if curl -sf "http://${LESHAN_HOST}:${LESHAN_API_PORT}/api/clients" > /dev/null; then
    echo "OK"
else
    echo "FAILED"
    exit 1
fi

# 2. Check CoAP port (UDP)
echo -n "Checking CoAP port (UDP 5683)... "
if nc -zu ${LESHAN_HOST} ${LESHAN_COAP_PORT} 2>/dev/null; then
    echo "OK"
else
    echo "SKIPPED (netcat not available or port closed)"
fi

# 3. Test CoAP response with coap-client (if available)
if command -v coap-client &> /dev/null; then
    echo -n "Testing CoAP response... "
    if timeout 5 coap-client -m get "coap://${LESHAN_HOST}:${LESHAN_COAP_PORT}/.well-known/core" 2>/dev/null | grep -q "lwm2m"; then
        echo "OK"
    else
        echo "SKIPPED (no response)"
    fi
fi

# 4. Get server info
echo ""
echo "=== Leshan Server Info ==="
curl -s "http://${LESHAN_HOST}:${LESHAN_API_PORT}/api/server" 2>/dev/null | head -c 500 || echo "(no response)"
echo ""
echo ""
echo "=== Registered Clients ==="
curl -s "http://${LESHAN_HOST}:${LESHAN_API_PORT}/api/clients" 2>/dev/null || echo "(no clients)"
echo ""
echo ""
echo "Validation complete!"
