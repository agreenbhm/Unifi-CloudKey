#!/bin/bash
set -euo pipefail

# Provisioning supports a downloaded archive on hosts without git. Updates use
# the published images selected by that installation's Compose file as well.
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
cd "$SCRIPT_DIR"

if command -v docker-compose >/dev/null 2>&1; then
    COMPOSE=(docker-compose)
elif docker compose version >/dev/null 2>&1; then
    COMPOSE=(docker compose)
else
    echo "Docker Compose is required; run provisioning first." >&2
    exit 1
fi
COMPOSE+=(-f "$SCRIPT_DIR/docker-compose.yml")

"${COMPOSE[@]}" config -q
echo "Pulling the images selected by $SCRIPT_DIR/docker-compose.yml..."
"${COMPOSE[@]}" pull

echo "Applying the update..."
"${COMPOSE[@]}" up -d
echo "Containers started. Network may take several minutes to become ready."
