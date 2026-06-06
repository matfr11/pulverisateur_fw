# Pulvérisateur agricole — Firmware ESP32

Système de contrôle embarqué pour pulvérisateur agricole à 3 cartes ESP32 communicant via MQTT.

---

## Architecture

```
CARTE_SERVEUR  (192.168.4.1 · pulve-srv.local)
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

## Fonctionnalités

### Interface Web
- Dashboard 3 slides : Unité de pompage · Automate · Vannes & phares arrière
- Jauges temps réel (débit, volume transféré, progression brassage, niveau cuve)
- **Mode jour / nuit** : bascule ☽/☀ dans le bandeau, choix persisté via `localStorage`
- **Version firmware** affichée dans le bandeau, synchronisée automatiquement avec `VERSION_FIRMWARE`
- Page Settings : modification de tous les paramètres (facteur K, sécurités, brassage, cuve)

### Automatismes
- **Transfert automatique** : pompe + vanne 3 voies, arrêt automatique sur volume cible
- **Brassage automatique** : cycles marche/pause configurables, suspendu pendant un transfert
- **Sécurité cuve vide** : détection débit bas avec délai configurable, réarmement manuel ou automatique

### OTA sans câble
Depuis la page Settings du serveur, flasher n'importe quelle carte sans USB :

| Bouton | Cible | Mécanisme |
|--------|-------|-----------|
| SERVEUR | Carte serveur | Upload direct |
| CARTE AVANT | `pulve-av.local` | Proxy streaming mDNS → mini serveur OTA |
| CARTE ARRIÈRE | `pulve-ar.local` | Proxy streaming mDNS → mini serveur OTA |

Le proxy streame le firmware par chunks de 4 KB — jamais plus de 4 KB en RAM simultanément.

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

./build_all.sh 6.1.4
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
| `test_capteurs.c` | 3 | Débitmètre OK/KO, réarmement après impulsions |

**Total : 64 tests.**

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
│   │   ├── protocole_wifi.c         # WiFi AP/STA, reconnexion par timer
│   │   └── protocole_mqtt.c         # Client MQTT + sérialisation JSON
│   └── broker_mqtt/
│       └── broker_mqtt.c            # Broker MQTT embarqué (MQTT 3.1.1)
│
├── carte_relais/
│   ├── main/
│   │   ├── board_config.h           # Sélection de la carte (AVANT/ARRIERE/SERVEUR)
│   │   ├── app_init.c               # Séquence d'initialisation et tâche principale
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

---

## Changelog

### v6.1.4
- Web UI : mode jour/nuit (☽/☀) avec persistance `localStorage`
- Web UI : note de version synchronisée avec `VERSION_FIRMWARE` à la compilation
- Fix : champ `master` du JSON `/status` retournait `"AR"` à tort sur la carte serveur

### v6.1.2 / v6.1.3
- Fix : facteur K débitmètre ignorait la NVS au démarrage (toujours 4.72 par défaut)
- Fix : suppression du check de version côté cartes relais — la carte serveur est la source autoritaire ; les mises à jour de paramètres ne pouvaient plus être appliquées si la NVS serveur avait été réinitialisée

### v6.1.1
- 9 correctifs issus de la revue de code :
  - WiFi : `vTaskDelay` en handler d'événement remplacé par `esp_timer` one-shot
  - MQTT : buffer JSON partagé → buffer local par fonction (thread-safe)
  - Web UI : mutex sur `s_etat_avant` / `s_etat_arriere`
  - Config : mutex sur `s_config` + copie locale avant usage
  - Broker MQTT : mutex dans `client_deconnecter()`, accept et comptage clients
  - Broker MQTT : enforcement keepalive (timeout × 1.5)
  - Broker MQTT : correction codec longueur dans `envoyer_suback()`
  - Automatismes : état `AUTO_TR_TERMINE` visible une itération avant reset
  - Capteurs : débitmètre marqué KO après `ABSENCE_TIMEOUT_CYCLES` sans impulsion
- Suite de tests portée à 64 tests (ajout `test_capteurs.c`)
- OTA proxy streaming serveur → cartes relais via mDNS
- Hostname mDNS par carte (`pulve-av.local`, `pulve-ar.local`, `pulve-srv.local`)
