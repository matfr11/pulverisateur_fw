# Pulvérisateur agricole — Firmware ESP32

Système de contrôle embarqué pour pulvérisateur agricole à 3 cartes ESP32 communicant via MQTT.

---

## Architecture

```
CARTE_SERVEUR  (192.168.4.1)
│  WiFi AP · Broker MQTT · Web UI · Proxy OTA
│
├── CARTE_AVANT   (pulve-av.local)
│   Pompe · Vanne 3 voies · Phares · Débitmètre
│   Automatismes : transfert, brassage, sécurité cuve vide
│
└── CARTE_ARRIERE (pulve-ar.local)
    Vannes motorisées · Phares · Sonde de niveau cuve
```

- **WiFi** : la carte serveur crée le point d'accès `PULVE_AP`. Les cartes AVANT et ARRIÈRE s'y connectent en mode STA.
- **MQTT** : broker embarqué sur le serveur (port 1883). Toutes les cartes publient leur état et reçoivent des commandes via MQTT.
- **Web UI** : accessible sur `http://192.168.4.1` depuis n'importe quel appareil connecté à `PULVE_AP`.
- **mDNS** : chaque carte annonce son hostname (`pulve-av.local`, `pulve-ar.local`, `pulve-srv.local`).

---

## Prérequis

- ESP-IDF **v5.5.2** installé en natif sur Linux/WSL
- 3 ESP32 (carte 4 relais, carte 8 relais, ESP32 nu)

### Installation ESP-IDF (WSL / Linux)

```bash
# Dépendances système
sudo apt install -y git wget flex bison gperf python3 python3-pip python3-venv \
  cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0

# Cloner ESP-IDF v5.5.2
mkdir -p ~/esp && cd ~/esp
git clone -b v5.5.2 --recursive https://github.com/espressif/esp-idf.git

# Installer les outils (toolchain xtensa, cmake, ninja…)
cd ~/esp/esp-idf && ./install.sh esp32

# Ajouter à ~/.bashrc pour usage permanent
echo "alias get_idf='. \$HOME/esp/esp-idf/export.sh'" >> ~/.bashrc
```

---

## Compiler les firmwares

### Compiler les 3 firmwares en une commande

```bash
# Depuis la racine du dépôt
get_idf   # charger l'environnement (ou source ~/esp/esp-idf/export.sh)

./build_all.sh 6.2
```

Le script :
1. Compile séquentiellement CARTE_AVANT, CARTE_ARRIERE, CARTE_SERVEUR
2. Modifie automatiquement `board_config.h` entre chaque build
3. Copie les binaires dans `carte_relais/binaires/carte_XX_vN.N.bin`
4. Restaure `board_config.h` à CARTE_SERVEUR

### Compiler manuellement une seule carte

```bash
get_idf
cd carte_relais

# 1. Choisir la carte dans board_config.h (décommenter la ligne correspondante)
#define CARTE_AVANT      1   ← ou CARTE_ARRIERE, ou CARTE_SERVEUR

# 2. Build
idf.py set-target esp32
idf.py build
```

---

## Flasher une carte

### Via USB (première installation)

```bash
get_idf
idf.py -C carte_relais -p /dev/ttyUSB0 flash
```

### Via OTA depuis la Web UI (mises à jour sans câble)

1. Connecter un PC au réseau `PULVE_AP`
2. Ouvrir `http://192.168.4.1` → onglet **Settings**
3. Section **MISE À JOUR FIRMWARE** : choisir le fichier `.bin` puis cliquer sur la carte cible

| Bouton | Cible |
|--------|-------|
| SERVEUR | Carte serveur elle-même |
| CARTE AVANT | `pulve-av.local` via proxy mDNS |
| CARTE ARRIÈRE | `pulve-ar.local` via proxy mDNS |

> Le proxy streame le firmware par chunks de 4 KB — jamais plus de 4 KB en RAM simultanément.

---

## Tests

Suite de tests unitaires compilés avec **gcc sur Linux/WSL** (sans matériel).

```bash
cd tests
make run
```

| Fichier | Tests | Couverture |
|---------|-------|------------|
| `test_broker_logic.c` | 16 | Wildcards MQTT `+` / `#`, codec longueur |
| `test_json.c` | 10 | Round-trips JSON (état avant, arrière, config) |
| `test_securites.c` | 12 | Machine à états cuve vide (injection de temps) |
| `test_automatismes.c` | 17 | Machines à états transfert et brassage |
| `test_ota_proxy.c` | 6 | Construction URL OTA, mapping hostname mDNS |

**Total : 61 tests.**

---

## Structure du dépôt

```
pulverisateur_fw/
├── build_all.sh                     # Script de build des 3 firmwares
├── commun/
│   ├── include/
│   │   ├── types_pulverisateur.h    # Types partagés, VERSION_FIRMWARE
│   │   └── mqtt_topics.h            # Topics MQTT, credentials WiFi
│   ├── protocoles/
│   │   ├── protocole_wifi.c         # WiFi AP/STA
│   │   └── protocole_mqtt.c         # Client MQTT + sérialisation JSON
│   └── broker_mqtt/
│       └── broker_mqtt.c            # Broker MQTT embarqué (MQTT 3.1.1)
│
├── carte_relais/
│   ├── main/
│   │   ├── board_config.h           # Sélection de la carte (AVANT/ARRIERE/SERVEUR)
│   │   ├── app_init.c               # Séquence d'initialisation
│   │   └── idf_component.yml        # Dépendance mdns (component manager)
│   ├── components/
│   │   ├── gestion_actionneurs/
│   │   ├── gestion_capteurs/
│   │   ├── gestion_automatismes/
│   │   ├── gestion_securites/
│   │   ├── gestion_configuration/
│   │   ├── gestion_web_ui/          # Serveur HTTP + Web UI + proxy OTA
│   │   └── gestion_ota/             # Mini serveur OTA (cartes relais)
│   ├── binaires/                    # Binaires compilés (.bin)
│   └── partitions.csv
│
└── tests/
    ├── Makefile
    ├── mocks/                       # Stubs ESP-IDF pour gcc Linux
    └── test_*.c
```
