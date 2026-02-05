#!/bin/bash
# Script d'installation Mosquitto pour le MASTER
# Système Pulvérisateur Agricole
# Version 1.0 - 2026-02-02

set -e

# Couleurs
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}======================================${NC}"
echo -e "${BLUE}  Installation Broker MQTT${NC}"
echo -e "${BLUE}  Système Pulvérisateur Agricole${NC}"
echo -e "${BLUE}======================================${NC}"
echo ""

# Vérifier si on est root
if [ "$EUID" -ne 0 ]; then 
    echo -e "${RED}Erreur: Ce script doit être exécuté en tant que root${NC}"
    echo "Utilisez: sudo $0"
    exit 1
fi

# Détecter le système
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
else
    echo -e "${RED}Système non reconnu${NC}"
    exit 1
fi

echo -e "${GREEN}Système détecté: $OS${NC}"
echo ""

# Installation selon le système
case $OS in
    ubuntu|debian|raspbian)
        echo -e "${YELLOW}Installation via apt...${NC}"
        apt-get update
        apt-get install -y mosquitto mosquitto-clients
        ;;
    fedora|centos|rhel)
        echo -e "${YELLOW}Installation via dnf/yum...${NC}"
        if command -v dnf &> /dev/null; then
            dnf install -y mosquitto
        else
            yum install -y mosquitto
        fi
        ;;
    arch)
        echo -e "${YELLOW}Installation via pacman...${NC}"
        pacman -Sy --noconfirm mosquitto
        ;;
    *)
        echo -e "${RED}Système non supporté: $OS${NC}"
        echo "Installez Mosquitto manuellement"
        exit 1
        ;;
esac

echo -e "${GREEN}✓ Mosquitto installé${NC}"
echo ""

# Configuration Mosquitto
echo -e "${YELLOW}Configuration Mosquitto...${NC}"

# Sauvegarder config existante
if [ -f /etc/mosquitto/mosquitto.conf ]; then
    cp /etc/mosquitto/mosquitto.conf /etc/mosquitto/mosquitto.conf.backup
    echo -e "${GREEN}✓ Configuration existante sauvegardée${NC}"
fi

# Créer nouvelle configuration
cat > /etc/mosquitto/mosquitto.conf << 'EOF'
# Configuration Mosquitto - Système Pulvérisateur Agricole
# Version 1.0 - 2026-02-02

# =============================================================================
# LISTENER
# =============================================================================

# Port MQTT standard
listener 1883

# Accepter connexions de toutes les interfaces
bind_address 0.0.0.0

# =============================================================================
# SÉCURITÉ
# =============================================================================

# Autoriser connexions anonymes (pas de mot de passe)
# ATTENTION: À sécuriser en production si réseau exposé
allow_anonymous true

# Limite de connexions simultanées
max_connections 10

# =============================================================================
# PERSISTANCE
# =============================================================================

# Désactiver persistance (pas critique pour ce système)
# Gain de performance et moins d'usure carte SD
persistence false

# Si persistance activée, dossier de stockage:
# persistence_location /var/lib/mosquitto/

# =============================================================================
# LOGGING
# =============================================================================

# Niveau de log
log_dest syslog
log_dest stdout
log_type error
log_type warning
log_type notice
log_type information

# Timestamps
log_timestamp true
log_timestamp_format %Y-%m-%d %H:%M:%S

# =============================================================================
# PERFORMANCE
# =============================================================================

# Taille max message (1 MB)
message_size_limit 1048576

# Timeout client inactif (60 secondes)
keepalive_interval 60

# =============================================================================
# QOS ET RETAIN
# =============================================================================

# Activer messages retained
retain_available true

# Taille max queue par client
max_queued_messages 1000

# =============================================================================
# MONITORING
# =============================================================================

# Activer topic système pour monitoring
sys_interval 10

# Topics système disponibles:
# $SYS/broker/version
# $SYS/broker/uptime
# $SYS/broker/clients/connected
# $SYS/broker/messages/received
# $SYS/broker/messages/sent
EOF

echo -e "${GREEN}✓ Configuration créée${NC}"
echo ""

# Activer et démarrer Mosquitto
echo -e "${YELLOW}Démarrage Mosquitto...${NC}"

# Activer au boot
systemctl enable mosquitto

# Démarrer le service
systemctl restart mosquitto

# Attendre démarrage
sleep 2

# Vérifier status
if systemctl is-active --quiet mosquitto; then
    echo -e "${GREEN}✓ Mosquitto démarré${NC}"
else
    echo -e "${RED}✗ Erreur démarrage Mosquitto${NC}"
    systemctl status mosquitto
    exit 1
fi

echo ""
echo -e "${BLUE}======================================${NC}"
echo -e "${BLUE}  INSTALLATION TERMINÉE${NC}"
echo -e "${BLUE}======================================${NC}"
echo ""

# Afficher informations
echo -e "${GREEN}Broker MQTT opérationnel:${NC}"
echo "  - IP: $(hostname -I | awk '{print $1}')"
echo "  - Port: 1883"
echo "  - Protocole: MQTT v3.1.1"
echo "  - Auth: Anonyme (pas de mot de passe)"
echo ""

echo -e "${YELLOW}Commandes utiles:${NC}"
echo "  Status:     sudo systemctl status mosquitto"
echo "  Arrêt:      sudo systemctl stop mosquitto"
echo "  Redémarrage:sudo systemctl restart mosquitto"
echo "  Logs:       sudo journalctl -u mosquitto -f"
echo ""

echo -e "${YELLOW}Test de connexion:${NC}"
echo "  # Terminal 1: Souscription"
echo "  mosquitto_sub -h localhost -t 'pulverisateur/#' -v"
echo ""
echo "  # Terminal 2: Publication"
echo "  mosquitto_pub -h localhost -t 'pulverisateur/test' -m 'Hello MQTT'"
echo ""

echo -e "${YELLOW}Monitoring:${NC}"
echo "  mosquitto_sub -h localhost -t '\$SYS/broker/#' -v"
echo ""

echo -e "${GREEN}Installation réussie ! 🎉${NC}"
