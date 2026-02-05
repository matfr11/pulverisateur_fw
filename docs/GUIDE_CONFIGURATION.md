# Guide de Configuration et Compilation - Système Pulvérisateur Agricole

## Vue d'ensemble du projet

Le système est composé de **3 cartes ESP32** communicant via MQTT:
- **Carte AVANT**: Pompe, vanne 3 voies, débitmètre, automatismes
- **Carte ARRIÈRE**: Vannes 3 fils (2m, bout de rampe), phares
- **Carte ÉCRAN**: ESP32-P4-C6 avec interface LVGL 7 pouces

## Architecture des dossiers

```
pulverisateur_fw/
├── commun/                      # Code partagé
│   ├── include/
│   │   └── types_communs.h      # Structures et énums
│   ├── gestion_mqtt/
│   │   └── mqtt_topics.h        # Définitions topics MQTT
│   └── protocoles/
│
├── carte_relais/                # Projet cartes AVANT et ARRIÈRE
│   ├── CMakeLists.txt           # Configuration CMake
│   ├── sdkconfig               # Configuration ESP-IDF
│   ├── main/
│   │   ├── CMakeLists.txt
│   │   ├── app_main.cpp         # Point d'entrée
│   │   ├── app_init.cpp/h       # Initialisations
│   │   ├── app_role.cpp/h       # Gestion Master/Slave
│   │   └── board_config.h       # Config matérielle
│   └── components/
│       ├── gestion_actionneurs/ # Pilotage relais
│       ├── gestion_capteurs/    # Lecture capteurs
│       ├── gestion_automatismes/# Transfert, brassage
│       ├── gestion_securites/   # Cuve vide, timeouts
│       └── gestion_configuration/# Stockage NVS
│
└── carte_ecran/                 # Projet ESP32-P4-C6
    ├── CMakeLists.txt
    ├── sdkconfig
    ├── main/
    │   └── app_main.cpp
    └── components/
        ├── ui_lvgl/             # Interface LVGL
        └── ui_mqtt/             # Liaison UI ↔ MQTT
```

## ÉTAPE 1: Installation et configuration ESP-IDF

### 1.1 Installer ESP-IDF v5.1 ou supérieur

```bash
# Cloner ESP-IDF
cd ~/
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout v5.1.2  # ou version stable récente

# Installer les outils
./install.sh esp32

# Activer l'environnement (à faire à chaque session)
. ./export.sh
```

### 1.2 Vérifier l'installation

```bash
idf.py --version
# Devrait afficher: ESP-IDF v5.1.2 ou supérieur
```

## ÉTAPE 2: Configuration du projet carte_relais

### 2.1 Compiler pour la carte AVANT

```bash
cd pulverisateur_fw/carte_relais

# Nettoyer
idf.py fullclean

# Configurer pour carte AVANT
idf.py -D BOARD_TYPE=AVANT set-target esp32

# Menuconfig (optionnel, pour ajuster WiFi, MQTT, etc.)
idf.py menuconfig

# Compiler
idf.py -D BOARD_TYPE=AVANT build

# Flasher
idf.py -p /dev/ttyUSB0 flash

# Moniteur série
idf.py -p /dev/ttyUSB0 monitor
```

### 2.2 Compiler pour la carte ARRIÈRE

```bash
cd pulverisateur_fw/carte_relais

# Nettoyer
idf.py fullclean

# Configurer pour carte ARRIÈRE
idf.py -D BOARD_TYPE=ARRIERE set-target esp32

# Compiler
idf.py -D BOARD_TYPE=ARRIERE build

# Flasher
idf.py -p /dev/ttyUSB1 flash

# Moniteur
idf.py -p /dev/ttyUSB1 monitor
```

### 2.3 Mode simulation (sans matériel)

Pour tester sans GPIO connectés:

```bash
# Activer le mode simulation
idf.py -D BOARD_TYPE=AVANT -D MODE_SIMULATION=ON build
```

## ÉTAPE 3: Configuration du projet carte_ecran

### 3.1 Prérequis LVGL

Le projet carte_ecran utilise LVGL 8.x avec ESP32-P4-C6.

```bash
cd pulverisateur_fw/carte_ecran

# Configurer pour ESP32-P4
idf.py set-target esp32p4

# Menuconfig pour configurer LVGL et l'écran
idf.py menuconfig
# → Component config → LVGL configuration
# → Écran: Résolution, driver, orientation

# Compiler
idf.py build

# Flasher
idf.py -p /dev/ttyUSB2 flash monitor
```

## ÉTAPE 4: Configuration menuconfig importante

### 4.1 Configuration WiFi

```
Component config → Wi-Fi
  → Maximum WiFi TX power (20 dBm recommandé)
  → WiFi Task Core ID (Core 0 ou 1)
```

### 4.2 Configuration MQTT

```
Component config → ESP-MQTT Configuration
  → Default MQTT version (3.1.1)
  → Enable custom event loop
```

### 4.3 FreeRTOS

```
Component config → FreeRTOS
  → Tick rate (Hz): 1000
  → Use FreeRTOS asserts: Yes (debug)
```

### 4.4 Logs

Pour développement:
```
Component config → Log output
  → Default log verbosity: Info
  → Maximum log verbosity: Verbose
```

Pour production:
```
  → Default log verbosity: Warning
```

## ÉTAPE 5: Première mise en route

### 5.1 Démarrage du système

1. **Flasher la carte AVANT** en premier
   - Elle détecte qu'aucun master n'existe
   - Elle devient MASTER
   - Elle génère le WiFi AP: `PulveriAG`
   - Elle démarre le broker MQTT

2. **Flasher la carte ARRIÈRE**
   - Elle détecte le WiFi existant
   - Elle devient SLAVE
   - Elle se connecte au master

3. **Flasher la carte ÉCRAN**
   - Se connecte au WiFi master
   - Affiche l'interface LVGL
   - Communique via MQTT

### 5.2 Vérification des logs

**Carte AVANT (Master):**
```
I (123) MAIN: >>> ROLE: MASTER <<<
I (456) WIFI: Point d'accès créé: PulveriAG
I (789) MQTT: Broker MQTT démarré sur port 1883
I (1000) MAIN: SYSTEME OPERATIONNEL
```

**Carte ARRIÈRE (Slave):**
```
I (123) MAIN: >>> ROLE: SLAVE <<<
I (456) WIFI: Connexion au master...
I (789) WIFI: Connecté! IP: 192.168.4.2
I (1000) MQTT: Connecté au broker 192.168.4.1
```

## ÉTAPE 6: Structure des composants (à implémenter)

Chaque composant doit avoir:

```
components/nom_composant/
├── CMakeLists.txt
├── include/
│   └── nom_composant.h
└── nom_composant.cpp
```

**Exemple CMakeLists.txt de composant:**
```cmake
idf_component_register(
    SRCS "nom_composant.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer mqtt
)
```

## ÉTAPE 7: Protocole de test

### Test 1: Communication MQTT

```bash
# Sur un PC connecté au WiFi PulveriAG
mosquitto_sub -h 192.168.4.1 -t "pulverisateur/#" -v

# Observer les messages
pulverisateur/presence/carte_avant {"online": true}
pulverisateur/etats/avant/pompe {"etat": 0}
```

### Test 2: Envoi de commandes

```bash
# Activer la pompe
mosquitto_pub -h 192.168.4.1 -t "pulverisateur/commandes/avant/pompe" -m '{"action":"ON"}'

# Vérifier l'état
mosquitto_sub -h 192.168.4.1 -t "pulverisateur/etats/avant/pompe"
```

## ÉTAPE 8: Debugging

### Activer les logs détaillés

```bash
# Dans menuconfig
Component config → Log output → Default log verbosity → Debug

# Ou dans le code (temporaire)
esp_log_level_set("*", ESP_LOG_DEBUG);
esp_log_level_set("MQTT", ESP_LOG_VERBOSE);
```

### Moniteur série amélioré

```bash
# Avec filtrage
idf.py monitor | grep "MQTT"

# Avec timestamp
idf.py monitor --print-filter "*:I"
```

### GDB debugging

```bash
idf.py openocd &
idf.py gdb
```

## ÉTAPE 9: Paramètres critiques

### board_config.h

Variables à ajuster selon votre matériel:

```cpp
// WiFi
#define WIFI_AP_SSID "PulveriAG"
#define WIFI_AP_PASSWORD "votreMotDePasse"

// GPIO (adapter selon votre câblage)
#define GPIO_RELAIS_POMPE GPIO_NUM_16

// Débitmètre
#define CONFIG_DEFAULT_FACTEUR_K_DEBITMETRE 4.72f
```

### Calibration débitmètre

1. Remplir 100L dans un récipient gradué
2. Compter les impulsions
3. Calculer: K = impulsions / litres
4. Mettre à jour dans la configuration

## ÉTAPE 10: Prochaines étapes d'implémentation

Les fichiers à créer en priorité:

1. **app_role.cpp/h** - Détection Master/Slave
2. **gestion_actionneurs/** - Pilotage GPIO
3. **gestion_capteurs/** - Débitmètre + ISR
4. **gestion_automatismes/** - Transfert + Brassage
5. **gestion_mqtt/** - Client MQTT + callbacks
6. **Interface LVGL** - Écrans + événements

## Commandes utiles

```bash
# Effacer la flash complète
idf.py -p /dev/ttyUSB0 erase-flash

# Effacer NVS uniquement
idf.py -p /dev/ttyUSB0 erase-nvs

# Moniteur avec reset
idf.py -p /dev/ttyUSB0 flash monitor

# Taille du binaire
idf.py size

# Partition table
idf.py partition-table
```

## Troubleshooting

### Erreur: "No module named 'serial'"
```bash
pip install pyserial
```

### Erreur: "CMake Error"
```bash
idf.py fullclean
rm -rf build
idf.py build
```

### Carte ne démarre pas
- Vérifier alimentation 5V/3.3V
- Vérifier GPIO0/BOOT au démarrage
- Effacer NVS: `idf.py erase-nvs`

### WiFi ne se connecte pas
- Vérifier SSID/Password dans board_config.h
- Augmenter timeout dans menuconfig
- Vérifier antenne WiFi connectée

---

## Support et Documentation

- ESP-IDF: https://docs.espressif.com/projects/esp-idf/
- LVGL: https://docs.lvgl.io/
- MQTT: https://docs.espressif.com/projects/esp-mqtt/

## Licence

Code propriétaire - Système pulvérisateur agricole
