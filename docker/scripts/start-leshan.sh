#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DOCKER_DIR="$(dirname "$SCRIPT_DIR")"

# Create logs directory if not exists
mkdir -p "${DOCKER_DIR}/../logs/leshan"

cd "$DOCKER_DIR"

echo "Starting Leshan LWM2M Server..."
docker compose up -d --build

echo "Waiting for Leshan to be ready..."
"${SCRIPT_DIR}/wait-for-leshan.sh"

echo ""
"${SCRIPT_DIR}/validate-connection.sh"
