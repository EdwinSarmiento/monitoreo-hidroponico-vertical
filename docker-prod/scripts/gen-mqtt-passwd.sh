#!/usr/bin/env bash
# Genera mosquitto/config/passwd para docker-prod/docker-compose.yml
# Ejecutar desde la carpeta docker-prod: ./scripts/gen-mqtt-passwd.sh <usuario> '<contraseña>'
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
USER="${1:-}"
PASS="${2:-}"
if [[ -z "$USER" || -z "$PASS" ]]; then
  echo "Uso: (cd docker-prod && ./scripts/gen-mqtt-passwd.sh <usuario> '<contraseña>')"
  echo "Ejemplo: ./scripts/gen-mqtt-passwd.sh hidro_mqtt 'ClaveSeguraLarga123!'"
  exit 1
fi
mkdir -p "$ROOT/mosquitto/config"
docker run --rm -v "$ROOT/mosquitto/config:/work" eclipse-mosquitto:2 \
  mosquitto_passwd -b -c /work/passwd "$USER" "$PASS"
chmod 600 "$ROOT/mosquitto/config/passwd" 2>/dev/null || true
echo "Listo: $ROOT/mosquitto/config/passwd (no lo subas a git)."
