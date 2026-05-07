#!/bin/bash
# ============================================================
#  Hidroponia IoT — Inicialización de certificados SSL
#  
#  Uso:
#    chmod +x init-ssl.sh
#    ./init-ssl.sh
#
#  Requisitos:
#    - Docker y Docker Compose instalados
#    - Puerto 80 y 443 disponibles
#    - Dominio apuntando a la IP de este servidor
# ============================================================

set -e

# Cargar variables de .env
if [ ! -f .env ]; then
    echo "ERROR: No se encontró el archivo .env"
    echo "Ejecuta: cp .env.example .env  y configura DOMAIN y EMAIL"
    exit 1
fi

source .env

if [ -z "$DOMAIN" ] || [ "$DOMAIN" = "hidroponia.ejemplo.com" ]; then
    echo "ERROR: Configura DOMAIN en .env con tu dominio real"
    exit 1
fi

if [ -z "$SSL_EMAIL" ] || [ "$SSL_EMAIL" = "tu-email@ejemplo.com" ]; then
    echo "ERROR: Configura SSL_EMAIL en .env con tu email real"
    exit 1
fi

SSL_DIR="./nginx/ssl"
CERTBOT_DIR="./certbot/www"
LETSENCRYPT_DIR="./certbot/conf"

echo "================================================"
echo "  Hidroponia IoT — Configuración SSL"
echo "  Dominio: $DOMAIN"
echo "  Email:   $SSL_EMAIL"
echo "================================================"

# Paso 1: Crear directorios
mkdir -p "$SSL_DIR" "$CERTBOT_DIR" "$LETSENCRYPT_DIR"

# Paso 2: Generar certificado temporal auto-firmado para que Nginx arranque
if [ ! -f "$SSL_DIR/fullchain.pem" ]; then
    echo ""
    echo "[1/4] Generando certificado temporal auto-firmado..."
    openssl req -x509 -nodes -newkey rsa:2048 -days 1 \
        -keyout "$SSL_DIR/privkey.pem" \
        -out "$SSL_DIR/fullchain.pem" \
        -subj "/CN=$DOMAIN" 2>/dev/null
    echo "      Certificado temporal creado."
else
    echo ""
    echo "[1/4] Certificado existente encontrado, se reemplazará."
fi

# Paso 3: Levantar Nginx con el certificado temporal
echo ""
echo "[2/4] Levantando servicios (Nginx + Mosquitto)..."
docker compose up -d nginx mosquitto
sleep 3

# Paso 4: Obtener certificado real con Certbot
echo ""
echo "[3/4] Solicitando certificado a Let's Encrypt..."
docker compose run --rm certbot certonly \
    --webroot \
    --webroot-path=/var/www/certbot \
    --email "$SSL_EMAIL" \
    --agree-tos \
    --no-eff-email \
    -d "$DOMAIN"

# Paso 5: Copiar certificados reales al volumen de Nginx
echo ""
echo "[4/4] Instalando certificado real..."
cp "$LETSENCRYPT_DIR/live/$DOMAIN/fullchain.pem" "$SSL_DIR/fullchain.pem"
cp "$LETSENCRYPT_DIR/live/$DOMAIN/privkey.pem" "$SSL_DIR/privkey.pem"

# Recargar Nginx con el certificado real
docker compose exec nginx nginx -s reload

echo ""
echo "================================================"
echo "  SSL configurado exitosamente!"
echo ""
echo "  Dashboard: https://$DOMAIN"
echo "  MQTT TCP:  $DOMAIN:${MQTT_PORT:-1883} (ESP32)"
echo "  MQTT WSS:  wss://$DOMAIN/mqtt (Dashboard)"
echo "================================================"
echo ""
echo "Los certificados se renuevan automáticamente."
echo "Para renovar manualmente: docker compose run --rm certbot renew"
