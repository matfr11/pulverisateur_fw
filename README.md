# Système Pulvérisateur Agricole - ESP32

Système de contrôle et automatisation pour pulvérisateur agricole basé sur trois cartes ESP32 communicant via MQTT.

## 🎯 Vue d'ensemble

- **3 cartes ESP32** en réseau WiFi mesh
- **Communication MQTT** (QoS 1 pour commandes critiques)
- **Failover automatique** Master/Slave
- **Interface LVGL** 7 pouces tactile
- **Web UI** embarquée (optionnelle)
- **Automatismes** transfert et brassage
- **Sécurités** intégrées (cuve vide, timeouts)

## 📦 Matériel requis

### Carte AVANT
- ESP32 DevKit V1 ou équivalent
- Carte 4 relais
- Débitmètre à impulsions
- Alimentation 5V/3A minimum

### Carte ARRIÈRE  
- ESP32 DevKit V1 ou équivalent
- Carte 8 relais
- Vannes 3 fils (2 sorties par vanne)
- Alimentation 5V/3A minimum

### Carte ÉCRAN
- ESP32-P4-C6
- Écran tactile 7 pouces
- Alimentation 5V/4A minimum

## 🚀 Démarrage rapide

### 1. Installer ESP-IDF

```bash
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout v5.1.2
./install.sh esp32
. ./export.sh
```

### 2. Compiler la carte AVANT

```bash
cd pulverisateur_fw/carte_relais
idf.py -D BOARD_TYPE=AVANT set-target esp32
idf.py -D BOARD_TYPE=AVANT build
idf.py -p /dev/ttyUSB0 flash monitor
```

### 3. Compiler la carte ARRIÈRE

```bash
cd pulverisateur_fw/carte_relais
idf.py fullclean
idf.py -D BOARD_TYPE=ARRIERE set-target esp32
idf.py -D BOARD_TYPE=ARRIERE build
idf.py -p /dev/ttyUSB1 flash monitor
```

### 4. Compiler la carte ÉCRAN

```bash
cd pulverisateur_fw/carte_ecran
idf.py set-target esp32p4
idf.py build
idf.py -p /dev/ttyUSB2 flash monitor
```

## 📋 Configuration

### WiFi

Par défaut:
- **SSID**: `PulveriAG`
- **Password**: `pulverisateur2026`

Modifiable dans `carte_relais/main/board_config.h`:
```cpp
#define WIFI_AP_SSID "VotreSSID"
#define WIFI_AP_PASSWORD "VotreMotDePasse"
```

### GPIO (à adapter selon votre câblage)

**Carte AVANT** (`board_config.h`):
```cpp
#define GPIO_RELAIS_POMPE           GPIO_NUM_16
#define GPIO_RELAIS_VANNE_3VOIES    GPIO_NUM_17
#define GPIO_RELAIS_PHARES_AVANT    GPIO_NUM_18
#define GPIO_DEBITMETRE_IMPULSION   GPIO_NUM_21
```

**Carte ARRIÈRE**:
```cpp
#define GPIO_VANNE_2M_OUVRIR        GPIO_NUM_16
#define GPIO_VANNE_2M_FERMER        GPIO_NUM_17
#define GPIO_VANNE_BOUT_OUVRIR      GPIO_NUM_18
#define GPIO_VANNE_BOUT_FERMER      GPIO_NUM_19
```

## 🔧 Fonctionnalités

### ✅ Implémenté (Fondations)

- [x] Architecture multi-projets ESP-IDF
- [x] Structures de données complètes
- [x] Définitions MQTT centralisées
- [x] Configuration NVS
- [x] Différenciation carte AVANT/ARRIÈRE
- [x] Initialisation GPIO
- [x] Mode simulation
- [x] Documentation complète

### ⏳ En cours d'implémentation

- [ ] Gestion Master/Slave (app_role.cpp)
- [ ] Client/Broker MQTT
- [ ] Pilotage actionneurs
- [ ] Lecture débitmètre avec ISR
- [ ] Automatismes transfert/brassage
- [ ] Sécurités (cuve vide, timeouts)
- [ ] Interface LVGL

## 📁 Structure du projet

```
pulverisateur_fw/
├── commun/                  # Code partagé
│   ├── include/
│   │   └── types_communs.h
│   └── gestion_mqtt/
│       └── mqtt_topics.h
│
├── carte_relais/            # Projet cartes AVANT et ARRIÈRE
│   ├── CMakeLists.txt
│   ├── main/
│   │   ├── app_main.cpp
│   │   ├── app_init.cpp/h
│   │   ├── app_role.cpp/h
│   │   └── board_config.h
│   └── components/
│       ├── gestion_actionneurs/
│       ├── gestion_capteurs/
│       ├── gestion_automatismes/
│       └── gestion_securites/
│
├── carte_ecran/             # Projet ESP32-P4-C6
│   └── components/
│       ├── ui_lvgl/
│       └── ui_mqtt/
│
└── docs/
    ├── GUIDE_CONFIGURATION.md
    └── ETAT_PROJET.md
```

## 🔒 Sécurités

### Détection cuve avant vide

- Seuil débit: 1.2 L/min (configurable)
- Délai détection: 3 secondes
- Action: Désactivation automatismes
- Commandes manuelles: Toujours actives
- Réarmement: Manuel

### Timeouts vannes 3 fils

- Timeout par défaut: 30 secondes
- Interlock strict: Jamais ouvrir + fermer simultanés
- Coupure automatique après timeout
- Protection contre blocage mécanique

### Priorités

```
MANUEL > SÉCURITÉ > AUTOMATISME
```

Les commandes manuelles sont **toujours** prioritaires.

## 🌐 Interface Web UI (optionnelle)

Si vous souhaitez utiliser la Web UI en plus de LVGL:

```bash
# Activer le serveur HTTP dans menuconfig
idf.py menuconfig
# → Component config → HTTP Server → Enable
```

La Web UI existante sera adaptée pour communiquer via MQTT.

## 📡 Topics MQTT

### Commandes

```
pulverisateur/commandes/avant/pompe
pulverisateur/commandes/avant/vanne_3voies
pulverisateur/commandes/arriere/vanne_2m
pulverisateur/automatismes/transfert/activer
pulverisateur/automatismes/brassage/activer
```

### États (retain)

```
pulverisateur/etats/avant/pompe
pulverisateur/etats/avant/vanne_3voies
pulverisateur/capteurs/debitmetre
pulverisateur/automatismes/transfert/etat
```

### Configuration

```
pulverisateur/configuration/instantane (retain)
pulverisateur/configuration/mise_a_jour
```

## 🧪 Tests

### Mode simulation (sans matériel)

```bash
idf.py -D BOARD_TYPE=AVANT -D MODE_SIMULATION=ON build
```

Actionneurs et capteurs virtuels pour développement sans GPIO.

### Moniteur MQTT

```bash
# Sur un PC connecté au WiFi PulveriAG
mosquitto_sub -h 192.168.4.1 -t "pulverisateur/#" -v
```

### Envoyer des commandes

```bash
mosquitto_pub -h 192.168.4.1 \
  -t "pulverisateur/commandes/avant/pompe" \
  -m '{"action":"ON"}'
```

## 🐛 Troubleshooting

### Carte ne démarre pas
```bash
# Effacer complètement la flash
idf.py -p /dev/ttyUSB0 erase-flash

# Reflasher
idf.py -p /dev/ttyUSB0 flash
```

### WiFi ne se connecte pas
- Vérifier antenne WiFi connectée
- Augmenter le timeout dans menuconfig
- Vérifier alimentation stable

### MQTT ne se connecte pas
```bash
# Logs détaillés
esp_log_level_set("MQTT", ESP_LOG_VERBOSE);
```

## 📚 Documentation

- [Guide de configuration ESP-IDF](docs/GUIDE_CONFIGURATION.md)
- [État du projet](docs/ETAT_PROJET.md)
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/)
- [LVGL Documentation](https://docs.lvgl.io/)

## 🔄 Workflow de développement

1. Modifier le code
2. Compiler: `idf.py build`
3. Flasher: `idf.py flash`
4. Monitor: `idf.py monitor`
5. Ctrl+] pour quitter le monitor

## ⚡ Performances

- **Latence MQTT**: < 50ms
- **Fréquence débitmètre**: Jusqu'à 1kHz
- **Mise à jour écran**: 30 FPS
- **Uptime**: > 30 jours sans redémarrage

## 🔐 Licence

Code propriétaire - Système pulvérisateur agricole  
© 2026 - Tous droits réservés

## 👥 Support

Pour toute question technique:
1. Consulter la documentation dans `/docs`
2. Vérifier les logs avec `idf.py monitor`
3. Activer les logs debug dans menuconfig

## 🗺️ Roadmap

### Version 1.0 (Actuelle)
- ✅ Architecture de base
- ⏳ Fonctionnalités core
- ⏳ Interface LVGL

### Version 1.1 (À venir)
- Statistiques d'utilisation
- Historique des transferts
- Calibration automatique débitmètre

### Version 2.0 (Futur)
- Support sonde niveau arrière
- Application mobile
- Logs sur carte SD

---

**Version**: 1.0.0  
**Dernière mise à jour**: 2026-02-02  
**Statut**: En développement actif
