# GUIDE DE SETUP ESP-IDF – PULVÉRISATEUR AGRICOLE

## 1. Installation de ESP-IDF

### 1.1 Prérequis système (Linux/WSL recommandé)
```bash
sudo apt-get install git wget flex bison gperf python3 python3-pip \
  python3-venv cmake ninja-build ccache libffi-dev libssl-dev \
  dfu-util libusb-1.0-0
```

### 1.2 Cloner ESP-IDF v5.3+ (branche stable)
```bash
mkdir -p ~/esp
cd ~/esp
git clone -b v5.3 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32,esp32p4
```

### 1.3 Activer l'environnement
```bash
# Ajouter à ~/.bashrc pour usage permanent :
alias get_idf='. $HOME/esp/esp-idf/export.sh'

# Puis à chaque session :
get_idf
```

### 1.4 Vérifier l'installation
```bash
idf.py --version
# Doit afficher : ESP-IDF v5.3.x
```

---

## 2. Structure du dépôt

```
pulverisateur_fw/
├── commun/                          # Code partagé (symlinké dans chaque projet)
│   ├── include/
│   │   ├── types_pulverisateur.h    # Structures, enums, constantes
│   │   └── mqtt_topics.h            # Définition des topics MQTT
│   ├── protocoles/
│   │   ├── protocole_wifi.h/.c      # Gestion WiFi AP/STA + failover
│   │   └── protocole_mqtt.h/.c      # Client MQTT + sérialisation JSON
│   └── gestion_mqtt/
│       └── mqtt_messages.h/.c       # Encodage/décodage des messages
│
├── carte_relais/                    # Projet ESP-IDF pour carte AVANT + ARRIÈRE
│   ├── CMakeLists.txt
│   ├── sdkconfig.defaults
│   ├── main/
│   │   ├── CMakeLists.txt
│   │   ├── app_main.c
│   │   ├── app_init.c/.h
│   │   └── board_config.h           # #define CARTE_AVANT ou CARTE_ARRIERE
│   └── components/
│       ├── gestion_actionneurs/     # Relais, vannes, interlocks
│       ├── gestion_capteurs/        # Débitmètre, sonde niveau
│       ├── gestion_automatismes/    # Transfert, brassage
│       ├── gestion_securites/       # Détection cuve vide
│       ├── gestion_configuration/   # NVS, paramètres JSON
│       └── gestion_web_ui/          # Serveur HTTP + pages HTML
│
└── tools/scripts/
```

---

## 3. Configuration du projet carte_relais

### 3.1 Configurer la cible
```bash
cd pulverisateur_fw/carte_relais
idf.py set-target esp32
```

### 3.2 Configurer le projet (menuconfig)
```bash
idf.py menuconfig
```

Paramètres importants :
- **Component config → ESP-MQTT → Enable MQTT broker** (si utilisation du broker embarqué)
- **Component config → LWIP → Max Sockets** : 16
- **Component config → HTTP Server → Max URI Handlers** : 20
- **Component config → Wi-Fi → WiFi AP max connections** : 4
- **Partition Table** : Custom partition table (partitions.csv)

### 3.3 Table de partitions personnalisée
Créer `carte_relais/partitions.csv` :
```csv
# Name,    Type,  SubType,  Offset,   Size
nvs,       data,  nvs,      0x9000,   0x6000
phy_init,  data,  phy,      0xf000,   0x1000
factory,   app,   factory,  0x10000,  0x1E0000
```

### 3.4 Compiler pour la CARTE AVANT
```bash
# Dans board_config.h, vérifier :
# #define CARTE_AVANT 1

idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### 3.5 Compiler pour la CARTE ARRIÈRE
```bash
# Dans board_config.h, changer :
# #define CARTE_ARRIERE 1

idf.py build
idf.py -p /dev/ttyUSB1 flash monitor
```

---

## 4. Dépendances et composants externes

### 4.1 Broker MQTT embarqué
On utilise le composant `esp_mqtt_cxx` natif d'ESP-IDF pour le client.
Pour le **broker embarqué**, on utilise le composant communautaire :
```bash
# Dans le répertoire du projet :
idf.py add-dependency "mdns"
```

> **Note** : Pour le broker MQTT embarqué, on utilise une implémentation
> minimaliste basée sur le composant `mosqlern` ou on intègre un micro-broker
> dans le code (voir `protocole_mqtt.c`). Alternative : utiliser le composant
> IDF `esp_mqtt` en mode broker si disponible dans la version choisie,
> ou intégrer le composant `esp-mqtt-broker` via le registre de composants.

### 4.2 cJSON
Inclus nativement dans ESP-IDF. Pas besoin d'installation.
```c
#include "cJSON.h"
```

### 4.3 Serveur HTTP
Inclus nativement dans ESP-IDF :
```c
#include "esp_http_server.h"
```

---

## 5. Workflow de développement

### 5.1 Compilation + Flash + Monitor (cycle rapide)
```bash
idf.py build flash monitor -p /dev/ttyUSB0
# Ctrl+] pour quitter le monitor
```

### 5.2 Mode simulation
Défini à la compilation dans `board_config.h` :
```c
// Décommenter pour activer la simulation
// #define MODE_SIMULATION 1
```

En mode simulation :
- Les GPIO ne sont pas configurées
- Les lectures capteurs retournent des valeurs synthétiques
- Les relais sont tracés dans les logs sans actionnement réel

### 5.3 Debugging série
```bash
idf.py monitor
# Les logs ESP_LOGx apparaissent en temps réel
```

---

## 6. Configuration WiFi et MQTT au démarrage

### Séquence de boot :
1. La carte démarre et scanne les réseaux WiFi
2. Si le SSID `PULVE_AP` existe → connexion en STA (SLAVE)
3. Sinon → création du point d'accès `PULVE_AP` (MASTER)
4. Le MASTER lance le broker MQTT sur `mqtt://192.168.4.1:1883`
5. Les SLAVES se connectent au broker
6. Chaque carte publie son état initial et souscrit aux topics pertinents

### Failover :
- Si un SLAVE perd la connexion WiFi pendant > 10s → il tente de devenir MASTER
- L'ancien MASTER, au redémarrage, détecte le réseau existant → reste SLAVE

---

## 7. Prochaines étapes

1. ✅ Créer la structure de dossiers
2. ✅ Écrire le code commun (types, MQTT)
3. ✅ Écrire les composants carte_relais
4. ✅ Adapter la Web UI existante
5. 🔲 Tests terrain
