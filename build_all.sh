#!/usr/bin/env bash
# Compile les 3 firmwares et copie les binaires dans carte_relais/binaires/
# Usage : ./build_all.sh [version]
#   Ex  : ./build_all.sh 6.2
set -e

VERSION=${1:-"6.1"}
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/carte_relais"
BIN_DIR="$PROJECT_DIR/binaires"
BUILD_DIR="/tmp/pulve_build"
BOARD_CONFIG="$PROJECT_DIR/main/board_config.h"
IDF_EXPORT="$HOME/esp/esp-idf/export.sh"

# Charger l'environnement ESP-IDF
if [ -z "$IDF_PATH" ]; then
    echo ">>> Chargement ESP-IDF..."
    # shellcheck disable=SC1090
    source "$IDF_EXPORT"
fi

mkdir -p "$BIN_DIR"

build_carte() {
    local carte=$1       # AVANT | ARRIERE | SERVEUR
    local outname=$2     # carte_av | carte_ar | carte_serv

    echo ""
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "  BUILD $carte"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

    # Mettre à jour board_config.h
    sed -i "s|^#define CARTE_AVANT.*|//#define CARTE_AVANT      1|" "$BOARD_CONFIG"
    sed -i "s|^#define CARTE_ARRIERE.*|//#define CARTE_ARRIERE    1|" "$BOARD_CONFIG"
    sed -i "s|^#define CARTE_SERVEUR.*|//#define CARTE_SERVEUR    1|" "$BOARD_CONFIG"
    sed -i "s|^//#define CARTE_${carte}.*|#define CARTE_${carte}|" "$BOARD_CONFIG"

    # Nettoyer et construire
    rm -rf "$BUILD_DIR"
    idf.py -C "$PROJECT_DIR" -B "$BUILD_DIR" set-target esp32
    idf.py -C "$PROJECT_DIR" -B "$BUILD_DIR" build

    # Copier le binaire
    local dest="$BIN_DIR/${outname}_v${VERSION}.bin"
    cp "$BUILD_DIR/carte_relais.bin" "$dest"
    echo ">>> $dest ($(du -h "$dest" | cut -f1))"
}

build_carte "AVANT"   "carte_av"
build_carte "ARRIERE" "carte_ar"
build_carte "SERVEUR" "carte_serv"

# Restaurer CARTE_SERVEUR dans board_config.h
sed -i "s|^#define CARTE_AVANT.*|//#define CARTE_AVANT      1|" "$BOARD_CONFIG"
sed -i "s|^#define CARTE_ARRIERE.*|//#define CARTE_ARRIERE    1|" "$BOARD_CONFIG"
sed -i "s|^//#define CARTE_SERVEUR.*|#define CARTE_SERVEUR    1|" "$BOARD_CONFIG"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  BUILD TERMINÉ — firmware v${VERSION}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
ls -lh "$BIN_DIR"/*"v${VERSION}"*.bin
