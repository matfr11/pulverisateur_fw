# Architecture MQTT - Système Pulvérisateur

## 📡 Vue d'ensemble

Le système utilise MQTT comme **bus de communication unique** entre toutes les cartes.

### Composants

```
┌─────────────────┐
│  CARTE AVANT    │ Client MQTT
│  (MASTER WiFi)  │ 192.168.4.1
│                 │
│  + Mosquitto    │ Broker MQTT :1883
└────────┬────────┘
         │
         │ WiFi AP "PulveriAG"
         │
    ┌────┴────┬──────────┐
    │         │          │
┌───▼───┐ ┌──▼────┐ ┌───▼───┐
│ARRIERE│ │ ÉCRAN │ │ Autre │
│SLAVE  │ │ SLAVE │ │ SLAVE │
└───────┘ └───────┘ └───────┘
Clients MQTT       Clients MQTT
192.168.4.2        192.168.4.3+
```

## 🔧 Architecture Technique

### Option retenue: Broker externe Mosquitto

**Pourquoi ?**
- ✅ Simple et robuste
- ✅ Faible consommation ressources
- ✅ Broker mature et testé
- ✅ Facile à installer
- ✅ Support complet MQTT v3.1.1

**Comment ?**
- Mosquitto tourne sur le master (ou PC externe)
- Toutes les cartes ESP32 sont des **clients MQTT**
- Installation via script fourni

### Alternatives non retenues

**Mini-broker embarqué ESP32:**
- ❌ Complexité élevée
- ❌ Ressources importantes (~100KB RAM)
- ❌ Maintenance difficile
- ❌ Risque de bugs

**Broker cloud:**
- ❌ Dépendance Internet
- ❌ Latence
- ❌ Coûts potentiels
- ❌ Complexité réseau

## 📦 Installation Mosquitto

### Sur Raspberry Pi / PC Linux (MASTER)

```bash
# Copier le script sur le master
scp tools/scripts/install_mosquitto.sh pi@192.168.4.1:~/

# Se connecter au master
ssh pi@192.168.4.1

# Exécuter l'installation
chmod +x install_mosquitto.sh
sudo ./install_mosquitto.sh

# Vérifier
sudo systemctl status mosquitto
```

### Configuration générée

```conf
# /etc/mosquitto/mosquitto.conf
listener 1883
bind_address 0.0.0.0
allow_anonymous true
max_connections 10
persistence false
log_dest stdout
```

### Sur ESP32 pur (sans Linux)

**Impossible** de faire tourner Mosquitto directement.

**Solutions:**
1. Utiliser un Raspberry Pi comme master
2. Utiliser un PC/serveur avec Mosquitto
3. Broker cloud MQTT (HiveMQ, CloudMQTT)

## 🔄 Flux MQTT

### Démarrage système

```
1. MASTER démarre
   → WiFi AP créé
   → Mosquitto démarre (port 1883)
   → Client MQTT se connecte à localhost
   → Souscrit aux topics commandes
   → Publie présence (retain)

2. SLAVE1 démarre
   → Se connecte au WiFi master
   → Client MQTT se connecte à 192.168.4.1:1883
   → Souscrit aux topics
   → Publie présence
   → Demande configuration

3. SLAVE2 démarre
   → Idem SLAVE1
```

### Publication état pompe (exemple)

```
[MASTER - Carte AVANT]
  1. GPIO pompe activé
  2. mqtt_publish(
       topic: "pulverisateur/etats/avant/pompe",
       payload: '{"etat":1,"timestamp":12345}',
       qos: 0,
       retain: true
     )

[Broker Mosquitto]
  3. Reçoit message
  4. Stocke (retain=true)
  5. Diffuse aux abonnés

[SLAVE - Carte ÉCRAN]
  6. Reçoit message (callback)
  7. Met à jour UI (bouton vert)

[NOUVEAU CLIENT]
  8. Se connecte
  9. Souscrit "pulverisateur/etats/#"
  10. Reçoit IMMÉDIATEMENT l'état pompe
      (grâce au retain)
```

### Commande manuelle (exemple)

```
[Carte ÉCRAN]
  Utilisateur appuie sur bouton pompe
  ↓
  mqtt_publish(
    topic: "pulverisateur/commandes/avant/pompe",
    payload: '{"action":"TOGGLE"}',
    qos: 1,
    retain: false
  )
  ↓
[Broker]
  Diffuse aux abonnés
  ↓
[Carte AVANT]
  Callback: mqtt_callback_commande_avant("pompe", '{"action":"TOGGLE"}')
  ↓
  Composant actionneurs traite la commande
  ↓
  Active/désactive GPIO
  ↓
  Publie nouvel état (retain)
  ↓
[Toutes les interfaces]
  Reçoivent état et mettent à jour UI
```

## 🧪 Tests MQTT

### Test 1: Vérifier broker

```bash
# Sur le master
sudo systemctl status mosquitto

# Devrait afficher:
# ● mosquitto.service - Mosquitto MQTT Broker
#    Active: active (running)
```

### Test 2: Souscription test

```bash
# Terminal 1: Écouter tous les messages
mosquitto_sub -h 192.168.4.1 -t 'pulverisateur/#' -v

# Devrait afficher les messages en temps réel
```

### Test 3: Publication test

```bash
# Terminal 2: Envoyer un message
mosquitto_pub -h 192.168.4.1 \
  -t 'pulverisateur/test' \
  -m 'Hello MQTT'

# Terminal 1 devrait afficher:
# pulverisateur/test Hello MQTT
```

### Test 4: Messages retained

```bash
# Publier avec retain
mosquitto_pub -h 192.168.4.1 \
  -t 'pulverisateur/etats/test' \
  -m '{"etat":1}' \
  -r

# Se déconnecter et se reconnecter
mosquitto_sub -h 192.168.4.1 \
  -t 'pulverisateur/etats/test'

# Devrait recevoir IMMÉDIATEMENT le message
# (même publié avant la connexion)
```

### Test 5: QoS

```bash
# QoS 0 (at most once)
mosquitto_pub -h 192.168.4.1 \
  -t 'pulverisateur/test/qos0' \
  -m 'QoS 0' \
  -q 0

# QoS 1 (at least once)
mosquitto_pub -h 192.168.4.1 \
  -t 'pulverisateur/test/qos1' \
  -m 'QoS 1' \
  -q 1

# QoS 2 (exactly once)
mosquitto_pub -h 192.168.4.1 \
  -t 'pulverisateur/test/qos2' \
  -m 'QoS 2' \
  -q 2
```

### Test 6: Monitoring broker

```bash
# Statistiques système
mosquitto_sub -h 192.168.4.1 -t '$SYS/broker/#' -v

# Affiche:
# $SYS/broker/version mosquitto version 2.0.x
# $SYS/broker/uptime 3600 seconds
# $SYS/broker/clients/connected 3
# $SYS/broker/messages/received 1234
# $SYS/broker/messages/sent 5678
```

## 📊 Topics utilisés

### Commandes (QoS 1, pas de retain)

```
pulverisateur/commandes/avant/pompe
pulverisateur/commandes/avant/vanne_3voies
pulverisateur/commandes/avant/phares
pulverisateur/commandes/arriere/vanne_2m
pulverisateur/commandes/arriere/vanne_bout_rampe
pulverisateur/commandes/arriere/phares
```

### États (QoS 0, retain)

```
pulverisateur/etats/avant/pompe
pulverisateur/etats/avant/vanne_3voies
pulverisateur/etats/avant/phares
pulverisateur/etats/arriere/vanne_2m
pulverisateur/etats/arriere/vanne_bout_rampe
pulverisateur/etats/arriere/phares
```

### Capteurs (QoS 0, pas de retain)

```
pulverisateur/capteurs/debitmetre
pulverisateur/capteurs/niveau_arriere
```

### Automatismes (QoS 1)

```
pulverisateur/automatismes/transfert/activer
pulverisateur/automatismes/transfert/desactiver
pulverisateur/automatismes/transfert/etat (retain)
pulverisateur/automatismes/brassage/activer
pulverisateur/automatismes/brassage/desactiver
pulverisateur/automatismes/brassage/etat (retain)
```

### Configuration (QoS 1, retain)

```
pulverisateur/configuration/instantane (retain)
pulverisateur/configuration/mise_a_jour
pulverisateur/configuration/demande
```

### Système

```
pulverisateur/presence/carte_avant (retain, LWT)
pulverisateur/presence/carte_arriere (retain, LWT)
pulverisateur/presence/carte_ecran (retain, LWT)
pulverisateur/systeme/version_protocole
pulverisateur/systeme/role
```

## 🔒 Sécurité

### Configuration actuelle (développement)

- ✅ Réseau WiFi fermé (WPA2)
- ⚠️ MQTT anonyme (pas de mot de passe)
- ⚠️ Pas de TLS/SSL

**Acceptable pour:**
- Environnement isolé (champ agricole)
- Réseau local uniquement
- Pas de données sensibles

### Production (recommandations)

```conf
# /etc/mosquitto/mosquitto.conf

# Activer authentification
allow_anonymous false
password_file /etc/mosquitto/passwd

# Activer TLS (optionnel)
listener 8883
cafile /etc/mosquitto/ca.crt
certfile /etc/mosquitto/server.crt
keyfile /etc/mosquitto/server.key
```

Créer utilisateurs:
```bash
mosquitto_passwd -c /etc/mosquitto/passwd pulve_user
```

## 📈 Performance

### Latence typique

- Message publié → reçu: **< 50 ms**
- Commande → réaction GPIO: **< 100 ms**
- Heartbeat: **5 secondes**

### Charge réseau

- Heartbeat: 3 cartes × 20 bytes × 0.2 Hz = **12 bytes/s**
- Débitmètre: 50 bytes × 10 Hz = **500 bytes/s**
- Commandes: sporadiques, négligeables

**Total: < 1 KB/s** (négligeable pour WiFi)

### Ressources broker

- RAM: ~5 MB
- CPU: < 5%
- Connexions: 10 max

## 🐛 Troubleshooting

### Broker ne démarre pas

```bash
# Vérifier logs
sudo journalctl -u mosquitto -n 50

# Vérifier port
sudo netstat -tulpn | grep 1883

# Tester config
mosquitto -c /etc/mosquitto/mosquitto.conf -v
```

### Client ESP32 ne se connecte pas

```bash
# Vérifier depuis ESP32 (logs)
I (xxx) MQTT: Connexion à mqtt://192.168.4.1:1883
E (xxx) MQTT: Erreur connexion

# Vérifier depuis PC
ping 192.168.4.1
telnet 192.168.4.1 1883
```

### Messages non reçus

1. Vérifier souscription topic
2. Vérifier QoS
3. Vérifier connexion MQTT
4. Vérifier logs broker

---

**Version**: 1.0  
**Broker**: Mosquitto 2.0+  
**Protocole**: MQTT v3.1.1  
**Statut**: Production-ready
