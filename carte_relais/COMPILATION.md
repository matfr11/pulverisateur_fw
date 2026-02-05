# Guide de Compilation Rapide

## Prérequis

1. **ESP-IDF v5.1 ou supérieur** installé et configuré
2. Variables d'environnement ESP-IDF sourcées

```bash
# Vérifier ESP-IDF
idf.py --version
# Devrait afficher: ESP-IDF v5.1 ou supérieur
```

## Compilation

### Carte AVANT (Master)

```bash
cd carte_relais

# Configuration
idf.py -D BOARD_TYPE=AVANT set-target esp32

idf.py -D BOARD_TYPE=Avant fullclean
# Compilation
idf.py -D BOARD_TYPE=AVANT build

# Flash (avec port série)
idf.py -D BOARD_TYPE=AVANT -p /dev/ttyUSB0 flash

# Monitor
idf.py -D BOARD_TYPE=AVANT -p /dev/ttyUSB0 monitor
```

### Carte ARRIÈRE (Slave)

```bash
cd carte_relais

# Configuration
idf.py -D BOARD_TYPE=ARRIERE set-target esp32

idf.py -D BOARD_TYPE=ARRIERE fullclean
# Compilation
idf.py -D BOARD_TYPE=ARRIERE build

# Flash
idf.py -D BOARD_TYPE=ARRIERE -p /dev/ttyUSB0 flash

# Monitor
idf.py -D BOARD_TYPE=ARRIERE -p /dev/ttyUSB0 monitor
```

## Mode Simulation

Pour compiler sans matériel (tests):

```bash
# Activer mode simulation dans menuconfig
idf.py menuconfig
# → Component config → Pulverisateur Config → Enable Simulation Mode

# Ou compiler avec:
idf.py -D MODE_SIMULATION=ON -D BOARD_TYPE=AVANT build
```

## Erreurs Communes

### Erreur: "BOARD_TYPE non défini"

**Solution**: Toujours spécifier `-D BOARD_TYPE=AVANT` ou `-D BOARD_TYPE=ARRIERE`

### Erreur: composant manquant

**Cause**: Composants non compilés ou dépendances manquantes

**Solution**: Vérifier que tous les composants sont dans `components/`

### Erreur: types_communs.h non trouvé

**Solution**: Vérifier que `commun/include/` existe et contient `types_communs.h` et `mqtt_topics.h`

## Nettoyage

```bash
# Nettoyage complet
idf.py fullclean

# Effacer configuration
rm -rf build sdkconfig
```

## Première Compilation

**Important**: La première compilation prendra plusieurs minutes.

Les compilations suivantes seront beaucoup plus rapides (compilation incrémentale).

## Taille Binaire Attendue

- Carte AVANT: ~900 KB
- Carte ARRIÈRE: ~750 KB

## Vérification Compilation Réussie

Si la compilation réussit, vous verrez:

```
Project build complete. To flash, run:
 idf.py -p (PORT) flash
or
 idf.py -p (PORT) flash monitor
```

## Problèmes Connus État Actuel

### ⚠️ À corriger avant compilation complète:

1. **Composant gestion_configuration** - référencé mais pas créé
2. **Fonctions externes** - certaines déclarations `extern` peuvent manquer d'implémentation
3. **Headers manquants** - possibles includes manquants

### Solution temporaire:

Commenter les parties non implémentées dans les CMakeLists.txt des composants si nécessaire.

## Test Compilation Partielle

Pour tester la compilation sans tout compiler:

```bash
# Compiler uniquement le main
cd carte_relais
idf.py -D BOARD_TYPE=AVANT reconfigure

# Vérifier erreurs sans compiler
idf.py -D BOARD_TYPE=AVANT build 2>&1 | head -50
```

Cela montrera les premières erreurs sans attendre la fin.
