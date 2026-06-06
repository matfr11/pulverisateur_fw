/**
 * @file broker_mqtt.c
 * @brief Broker MQTT 3.1.1 embarqué pour ESP32.
 *
 * Architecture :
 * - 1 tâche "broker_accept" : accepte les connexions TCP
 * - 1 tâche "broker_loop"   : polling des sockets (select) + traitement paquets
 * - Structures internes pour clients, souscriptions et messages retain
 */
#include "broker_mqtt.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include <string.h>
#include <errno.h>

static const char *TAG = "BROKER";

/* ====================================================================
 * CONSTANTES PROTOCOLE MQTT
 * ==================================================================== */
#define MQTT_CONNECT        1
#define MQTT_CONNACK        2
#define MQTT_PUBLISH        3
#define MQTT_PUBACK         4
#define MQTT_SUBSCRIBE      8
#define MQTT_SUBACK         9
#define MQTT_UNSUBSCRIBE    10
#define MQTT_UNSUBACK       11
#define MQTT_PINGREQ        12
#define MQTT_PINGRESP       13
#define MQTT_DISCONNECT     14

/* ====================================================================
 * STRUCTURES INTERNES
 * ==================================================================== */

/** Client connecté */
typedef struct {
    int      sock;                                      /**< Socket (-1 = libre) */
    bool     connecte;                                  /**< CONNECT reçu et accepté */
    char     client_id[64];                             /**< Client ID */
    uint8_t  buffer[BROKER_TAILLE_BUFFER];              /**< Buffer de réception */
    int      buf_len;                                   /**< Données dans le buffer */
    int64_t  dernier_paquet_ms;                         /**< Timestamp du dernier paquet */
    uint16_t keepalive_s;                               /**< Keepalive annoncé (0 = désactivé) */
} broker_client_t;

/** Souscription */
typedef struct {
    int     client_idx;                                 /**< Index du client (-1 = libre) */
    char    filtre[BROKER_TAILLE_TOPIC_MAX];            /**< Filtre de topic (avec wildcards) */
    uint8_t qos;                                        /**< QoS demandé */
} broker_souscription_t;

/** Message retain */
typedef struct {
    bool    actif;
    char    topic[BROKER_TAILLE_TOPIC_MAX];
    uint8_t payload[BROKER_TAILLE_PAYLOAD_MAX];
    int     payload_len;
    uint8_t qos;
} broker_retain_t;

/* ====================================================================
 * VARIABLES STATIQUES
 * ==================================================================== */
static broker_client_t          s_clients[BROKER_MAX_CLIENTS];
static broker_souscription_t    s_souscriptions[BROKER_MAX_SOUSCRIPTIONS];
static broker_retain_t          s_retain[BROKER_MAX_RETAIN];

static int              s_socket_ecoute = -1;
static bool             s_actif = false;
static TaskHandle_t     s_tache_handle = NULL;
static SemaphoreHandle_t s_mutex = NULL;

/* ====================================================================
 * PROTOTYPES INTERNES
 * ==================================================================== */
static void     broker_tache(void *param);
static void     client_deconnecter(int idx);
static int      client_trouver_libre(void);
static int      paquet_lire_longueur_restante(const uint8_t *buf, int buf_len, int *octets_lus);
static bool     topic_correspond(const char *filtre, const char *topic);
static void     traiter_paquet(int client_idx, uint8_t *paquet, int longueur_totale);
static void     traiter_connect(int idx, uint8_t *payload, int len);
static void     traiter_publish(int idx, uint8_t *paquet, int longueur_totale);
static void     traiter_subscribe(int idx, uint8_t *payload, int len, uint16_t packet_id);
static void     traiter_unsubscribe(int idx, uint8_t *payload, int len, uint16_t packet_id);
static void     envoyer_connack(int idx, uint8_t code_retour);
static void     envoyer_puback(int idx, uint16_t packet_id);
static void     envoyer_suback(int idx, uint16_t packet_id, uint8_t *codes, int nb_codes);
static void     envoyer_unsuback(int idx, uint16_t packet_id);
static void     envoyer_pingresp(int idx);
static void     diffuser_publish(const char *topic, const uint8_t *payload, int payload_len,
                                 uint8_t qos, bool retain, int expediteur_idx);
static void     stocker_retain(const char *topic, const uint8_t *payload, int payload_len, uint8_t qos);
static void     envoyer_retain_pour_client(int client_idx);
static int      envoyer_raw(int sock, const uint8_t *data, int len);

/* ====================================================================
 * FONCTIONS UTILITAIRES MQTT
 * ==================================================================== */

/**
 * Décode la "Remaining Length" MQTT (encodage variable 1-4 octets).
 * Retourne la longueur, met dans *octets_lus le nombre d'octets consommés.
 * Retourne -1 si pas assez de données.
 */
static int paquet_lire_longueur_restante(const uint8_t *buf, int buf_len, int *octets_lus)
{
    int multiplier = 1;
    int valeur = 0;
    int idx = 0;

    do {
        if (idx >= buf_len) return -1;  /* Pas assez de données */
        uint8_t octet = buf[idx++];
        valeur += (octet & 0x7F) * multiplier;
        multiplier *= 128;
        if (!(octet & 0x80)) {
            *octets_lus = idx;
            return valeur;
        }
        if (multiplier > 128 * 128 * 128) return -1;  /* Mal formé */
    } while (1);
}

/**
 * Encode la "Remaining Length" MQTT.
 * Retourne le nombre d'octets écrits dans buf.
 */
static int encoder_longueur_restante(uint8_t *buf, int valeur)
{
    int idx = 0;
    do {
        uint8_t octet = valeur % 128;
        valeur /= 128;
        if (valeur > 0) octet |= 0x80;
        buf[idx++] = octet;
    } while (valeur > 0);
    return idx;
}

/**
 * Vérifie si un filtre de souscription (avec +/#) correspond à un topic.
 */
static bool topic_correspond(const char *filtre, const char *topic)
{
    while (*filtre && *topic) {
        if (*filtre == '#') {
            return true;  /* # matche tout le reste */
        }
        if (*filtre == '+') {
            /* + matche un seul niveau : avancer jusqu'au prochain / */
            while (*topic && *topic != '/') topic++;
            filtre++;
            continue;
        }
        if (*filtre != *topic) return false;
        filtre++;
        topic++;
    }

    /* Cas spécial : filtre "a/b/#" matche aussi "a/b" */
    if (*filtre == '/' && *(filtre + 1) == '#' && !*(filtre + 2)) return true;

    /* Les deux doivent être à la fin */
    if (*filtre == '#') return true;
    return (*filtre == '\0' && *topic == '\0');
}

/* ====================================================================
 * GESTION DES CLIENTS
 * ==================================================================== */

static int client_trouver_libre(void)
{
    for (int i = 0; i < BROKER_MAX_CLIENTS; i++) {
        if (s_clients[i].sock < 0) return i;
    }
    return -1;
}

static void client_deconnecter(int idx)
{
    if (idx < 0 || idx >= BROKER_MAX_CLIENTS) return;

    int sock_a_fermer = -1;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    broker_client_t *c = &s_clients[idx];
    if (c->sock >= 0) {
        sock_a_fermer = c->sock;
        ESP_LOGI(TAG, "Client [%d] '%s' déconnecté.", idx, c->client_id);
    }
    for (int i = 0; i < BROKER_MAX_SOUSCRIPTIONS; i++) {
        if (s_souscriptions[i].client_idx == idx) {
            s_souscriptions[i].client_idx = -1;
            s_souscriptions[i].filtre[0] = '\0';
        }
    }
    c->sock = -1;
    c->connecte = false;
    c->client_id[0] = '\0';
    c->buf_len = 0;
    xSemaphoreGive(s_mutex);

    if (sock_a_fermer >= 0) {
        close(sock_a_fermer);
    }
}

/* ====================================================================
 * TRAITEMENT DES PAQUETS
 * ==================================================================== */

static void traiter_paquet(int idx, uint8_t *paquet, int longueur_totale)
{
    uint8_t type = (paquet[0] >> 4) & 0x0F;

    /* Décoder remaining length */
    int octets_rl;
    int remaining = paquet_lire_longueur_restante(paquet + 1, longueur_totale - 1, &octets_rl);
    if (remaining < 0) return;

    int debut_payload = 1 + octets_rl;
    uint8_t *payload = paquet + debut_payload;

    s_clients[idx].dernier_paquet_ms = esp_timer_get_time() / 1000;

    switch (type) {
    case MQTT_CONNECT:
        traiter_connect(idx, payload, remaining);
        break;

    case MQTT_PUBLISH:
        traiter_publish(idx, paquet, longueur_totale);
        break;

    case MQTT_SUBSCRIBE: {
        if (remaining < 2) break;
        uint16_t packet_id = (payload[0] << 8) | payload[1];
        traiter_subscribe(idx, payload + 2, remaining - 2, packet_id);
        break;
    }

    case MQTT_UNSUBSCRIBE: {
        if (remaining < 2) break;
        uint16_t packet_id = (payload[0] << 8) | payload[1];
        traiter_unsubscribe(idx, payload + 2, remaining - 2, packet_id);
        break;
    }

    case MQTT_PINGREQ:
        envoyer_pingresp(idx);
        break;

    case MQTT_DISCONNECT:
        ESP_LOGI(TAG, "Client [%d] DISCONNECT propre.", idx);
        client_deconnecter(idx);
        break;

    case MQTT_PUBACK:
        /* QoS 1 : le client confirme réception. Rien à faire côté broker simple. */
        break;

    default:
        ESP_LOGW(TAG, "Type paquet non géré : %d", type);
        break;
    }
}

static void traiter_connect(int idx, uint8_t *payload, int len)
{
    /* Vérification minimale du paquet CONNECT */
    if (len < 10) {
        envoyer_connack(idx, 0x01);  /* Protocole inacceptable */
        return;
    }

    /* Protocol Name Length + "MQTT" */
    uint16_t proto_len = (payload[0] << 8) | payload[1];
    if (proto_len != 4 || memcmp(payload + 2, "MQTT", 4) != 0) {
        envoyer_connack(idx, 0x01);
        return;
    }

    /* Protocol Level (4 = MQTT 3.1.1) */
    /* uint8_t level = payload[6]; — on accepte tout */

    /* Connect Flags */
    /* uint8_t flags = payload[7]; */

    /* Keep Alive */
    uint16_t keepalive = (uint16_t)((payload[8] << 8) | payload[9]);
    s_clients[idx].keepalive_s = keepalive;

    /* Client ID */
    int pos = 10;
    if (pos + 2 > len) {
        envoyer_connack(idx, 0x02);
        return;
    }
    uint16_t id_len = (payload[pos] << 8) | payload[pos + 1];
    pos += 2;

    if (pos + id_len > len || id_len >= sizeof(s_clients[idx].client_id)) {
        envoyer_connack(idx, 0x02);
        return;
    }

    memcpy(s_clients[idx].client_id, payload + pos, id_len);
    s_clients[idx].client_id[id_len] = '\0';
    s_clients[idx].connecte = true;

    ESP_LOGI(TAG, "Client [%d] CONNECT : '%s'", idx, s_clients[idx].client_id);
    envoyer_connack(idx, 0x00);  /* Succès */

    /* Envoyer les messages retain correspondant aux futures souscriptions */
    /* (sera fait lors du SUBSCRIBE) */
}

static void traiter_publish(int idx, uint8_t *paquet, int longueur_totale)
{
    uint8_t flags = paquet[0] & 0x0F;
    bool    dup    = (flags >> 3) & 0x01;
    uint8_t qos    = (flags >> 1) & 0x03;
    bool    retain = flags & 0x01;

    (void)dup;

    /* Décoder remaining length */
    int octets_rl;
    int remaining = paquet_lire_longueur_restante(paquet + 1, longueur_totale - 1, &octets_rl);
    if (remaining < 2) return;

    int pos = 1 + octets_rl;

    /* Topic */
    uint16_t topic_len = (paquet[pos] << 8) | paquet[pos + 1];
    pos += 2;
    if (topic_len >= BROKER_TAILLE_TOPIC_MAX || pos + topic_len > longueur_totale) return;

    char topic[BROKER_TAILLE_TOPIC_MAX];
    memcpy(topic, paquet + pos, topic_len);
    topic[topic_len] = '\0';
    pos += topic_len;

    /* Packet ID (QoS >= 1) */
    uint16_t packet_id = 0;
    if (qos >= 1) {
        if (pos + 2 > longueur_totale) return;
        packet_id = (paquet[pos] << 8) | paquet[pos + 1];
        pos += 2;
    }

    /* Payload */
    int payload_len = remaining - (2 + topic_len + (qos >= 1 ? 2 : 0));
    if (payload_len < 0) payload_len = 0;
    if (payload_len > BROKER_TAILLE_PAYLOAD_MAX) payload_len = BROKER_TAILLE_PAYLOAD_MAX;

    const uint8_t *payload = paquet + pos;

    /* QoS 1 : envoyer PUBACK */
    if (qos == 1) {
        envoyer_puback(idx, packet_id);
    }

    /* Retain : stocker ou supprimer */
    if (retain) {
        if (payload_len > 0) {
            stocker_retain(topic, payload, payload_len, qos);
        } else {
            /* Payload vide + retain = supprimer le retain */
            for (int i = 0; i < BROKER_MAX_RETAIN; i++) {
                if (s_retain[i].actif && strcmp(s_retain[i].topic, topic) == 0) {
                    s_retain[i].actif = false;
                    ESP_LOGD(TAG, "Retain supprimé : %s", topic);
                    break;
                }
            }
        }
    }

    /* Diffuser aux souscripteurs */
    diffuser_publish(topic, payload, payload_len, qos, false, idx);
}

static void traiter_subscribe(int idx, uint8_t *payload, int len, uint16_t packet_id)
{
    uint8_t codes_retour[BROKER_MAX_SOUSCRIPTIONS];
    int nb_codes = 0;
    int pos = 0;

    while (pos < len && nb_codes < BROKER_MAX_SOUSCRIPTIONS) {
        if (pos + 2 > len) break;
        uint16_t filtre_len = (payload[pos] << 8) | payload[pos + 1];
        pos += 2;

        if (pos + filtre_len + 1 > len) break;  /* +1 pour le QoS */
        if (filtre_len >= BROKER_TAILLE_TOPIC_MAX) {
            pos += filtre_len + 1;
            codes_retour[nb_codes++] = 0x80;  /* Échec */
            continue;
        }

        char filtre[BROKER_TAILLE_TOPIC_MAX];
        memcpy(filtre, payload + pos, filtre_len);
        filtre[filtre_len] = '\0';
        pos += filtre_len;

        uint8_t qos_demande = payload[pos++] & 0x03;

        /* Chercher un slot libre pour la souscription */
        int slot = -1;
        for (int i = 0; i < BROKER_MAX_SOUSCRIPTIONS; i++) {
            /* Remplacer si même client + même filtre */
            if (s_souscriptions[i].client_idx == idx &&
                strcmp(s_souscriptions[i].filtre, filtre) == 0) {
                slot = i;
                break;
            }
            if (slot < 0 && s_souscriptions[i].client_idx < 0) {
                slot = i;
            }
        }

        if (slot >= 0) {
            s_souscriptions[slot].client_idx = idx;
            strlcpy(s_souscriptions[slot].filtre, filtre, BROKER_TAILLE_TOPIC_MAX);
            s_souscriptions[slot].qos = qos_demande > 1 ? 1 : qos_demande;  /* Cap QoS à 1 */
            codes_retour[nb_codes++] = s_souscriptions[slot].qos;
            ESP_LOGI(TAG, "Client [%d] SUBSCRIBE '%s' QoS %d", idx, filtre, s_souscriptions[slot].qos);
        } else {
            ESP_LOGW(TAG, "Plus de slots pour souscription !");
            codes_retour[nb_codes++] = 0x80;
        }
    }

    envoyer_suback(idx, packet_id, codes_retour, nb_codes);

    /* Envoyer les messages retain qui correspondent aux nouveaux filtres */
    envoyer_retain_pour_client(idx);
}

static void traiter_unsubscribe(int idx, uint8_t *payload, int len, uint16_t packet_id)
{
    int pos = 0;

    while (pos < len) {
        if (pos + 2 > len) break;
        uint16_t filtre_len = (payload[pos] << 8) | payload[pos + 1];
        pos += 2;
        if (pos + filtre_len > len) break;

        char filtre[BROKER_TAILLE_TOPIC_MAX];
        int copy_len = filtre_len < BROKER_TAILLE_TOPIC_MAX - 1 ? filtre_len : BROKER_TAILLE_TOPIC_MAX - 1;
        memcpy(filtre, payload + pos, copy_len);
        filtre[copy_len] = '\0';
        pos += filtre_len;

        for (int i = 0; i < BROKER_MAX_SOUSCRIPTIONS; i++) {
            if (s_souscriptions[i].client_idx == idx &&
                strcmp(s_souscriptions[i].filtre, filtre) == 0) {
                s_souscriptions[i].client_idx = -1;
                ESP_LOGI(TAG, "Client [%d] UNSUBSCRIBE '%s'", idx, filtre);
                break;
            }
        }
    }

    envoyer_unsuback(idx, packet_id);
}

/* ====================================================================
 * CONSTRUCTION ET ENVOI DE PAQUETS
 * ==================================================================== */

static int envoyer_raw(int sock, const uint8_t *data, int len)
{
    if (sock < 0) return -1;
    int total = 0;
    while (total < len) {
        int n = send(sock, data + total, len - total, MSG_NOSIGNAL);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
}

static void envoyer_connack(int idx, uint8_t code_retour)
{
    uint8_t pkt[] = { (MQTT_CONNACK << 4), 2, 0x00, code_retour };
    envoyer_raw(s_clients[idx].sock, pkt, sizeof(pkt));
}

static void envoyer_puback(int idx, uint16_t packet_id)
{
    uint8_t pkt[] = { (MQTT_PUBACK << 4), 2, (uint8_t)(packet_id >> 8), (uint8_t)(packet_id & 0xFF) };
    envoyer_raw(s_clients[idx].sock, pkt, sizeof(pkt));
}

static void envoyer_suback(int idx, uint16_t packet_id, uint8_t *codes, int nb_codes)
{
    /* remaining = 2 (packet_id) + nb_codes (codes retour), peut dépasser 127 octets */
    uint8_t rl_buf[4];
    int rl_len = encoder_longueur_restante(rl_buf, 2 + nb_codes);

    uint8_t header[1 + 4 + 2];  /* type + rl (max 4 octets) + packet_id */
    header[0] = (MQTT_SUBACK << 4);
    memcpy(header + 1, rl_buf, rl_len);
    header[1 + rl_len]     = (uint8_t)(packet_id >> 8);
    header[1 + rl_len + 1] = (uint8_t)(packet_id & 0xFF);

    envoyer_raw(s_clients[idx].sock, header, 1 + rl_len + 2);
    envoyer_raw(s_clients[idx].sock, codes, nb_codes);
}

static void envoyer_unsuback(int idx, uint16_t packet_id)
{
    uint8_t pkt[] = { (MQTT_UNSUBACK << 4), 2, (uint8_t)(packet_id >> 8), (uint8_t)(packet_id & 0xFF) };
    envoyer_raw(s_clients[idx].sock, pkt, sizeof(pkt));
}

static void envoyer_pingresp(int idx)
{
    uint8_t pkt[] = { (MQTT_PINGRESP << 4), 0 };
    envoyer_raw(s_clients[idx].sock, pkt, sizeof(pkt));
}

/**
 * Construit un paquet PUBLISH et l'envoie à un socket.
 */
static int envoyer_publish(int sock, const char *topic, const uint8_t *payload,
                           int payload_len, uint8_t qos, uint16_t packet_id)
{
    uint16_t topic_len = (uint16_t)strlen(topic);
    int remaining = 2 + topic_len + payload_len + (qos >= 1 ? 2 : 0);

    uint8_t header[5];
    header[0] = (MQTT_PUBLISH << 4) | (qos << 1);
    int rl_len = encoder_longueur_restante(header + 1, remaining);

    /* Envoyer header + remaining length */
    if (envoyer_raw(sock, header, 1 + rl_len) < 0) return -1;

    /* Envoyer topic length + topic */
    uint8_t tl[2] = { (uint8_t)(topic_len >> 8), (uint8_t)(topic_len & 0xFF) };
    if (envoyer_raw(sock, tl, 2) < 0) return -1;
    if (envoyer_raw(sock, (const uint8_t *)topic, topic_len) < 0) return -1;

    /* Packet ID pour QoS 1 */
    if (qos >= 1) {
        uint8_t pid[2] = { (uint8_t)(packet_id >> 8), (uint8_t)(packet_id & 0xFF) };
        if (envoyer_raw(sock, pid, 2) < 0) return -1;
    }

    /* Payload */
    if (payload_len > 0) {
        if (envoyer_raw(sock, payload, payload_len) < 0) return -1;
    }

    return 0;
}

/* ====================================================================
 * DIFFUSION ET RETAIN
 * ==================================================================== */

static uint16_t s_next_packet_id = 1;

static void diffuser_publish(const char *topic, const uint8_t *payload, int payload_len,
                             uint8_t qos, bool retain_flag, int expediteur_idx)
{
    for (int i = 0; i < BROKER_MAX_SOUSCRIPTIONS; i++) {
        int cidx = s_souscriptions[i].client_idx;
        if (cidx < 0) continue;
        if (!s_clients[cidx].connecte) continue;
        /*
         * Ne pas renvoyer le message à son propre expéditeur.
         * Sans ce filtre, le serveur recevrait sa propre config publiée
         * et la republierait indéfiniment (boucle infinie).
         * C'est l'équivalent de l'option "No Local" de MQTT 5.0.
         */
        if (cidx == expediteur_idx) continue;

        if (topic_correspond(s_souscriptions[i].filtre, topic)) {
            /* QoS effectif = min(QoS publié, QoS souscrit) */
            uint8_t qos_eff = qos < s_souscriptions[i].qos ? qos : s_souscriptions[i].qos;
            uint16_t pid = (qos_eff >= 1) ? s_next_packet_id++ : 0;
            if (s_next_packet_id == 0) s_next_packet_id = 1;

            if (envoyer_publish(s_clients[cidx].sock, topic, payload, payload_len, qos_eff, pid) < 0) {
                ESP_LOGW(TAG, "Erreur envoi vers client [%d], déconnexion.", cidx);
                client_deconnecter(cidx);
            }
        }
    }
}

static void stocker_retain(const char *topic, const uint8_t *payload, int payload_len, uint8_t qos)
{
    /* Chercher si ce topic a déjà un retain */
    int slot = -1;
    for (int i = 0; i < BROKER_MAX_RETAIN; i++) {
        if (s_retain[i].actif && strcmp(s_retain[i].topic, topic) == 0) {
            slot = i;
            break;
        }
    }

    /* Sinon, chercher un slot libre */
    if (slot < 0) {
        for (int i = 0; i < BROKER_MAX_RETAIN; i++) {
            if (!s_retain[i].actif) {
                slot = i;
                break;
            }
        }
    }

    if (slot < 0) {
        ESP_LOGW(TAG, "Plus de slots retain disponibles !");
        return;
    }

    s_retain[slot].actif = true;
    strlcpy(s_retain[slot].topic, topic, BROKER_TAILLE_TOPIC_MAX);
    memcpy(s_retain[slot].payload, payload, payload_len < BROKER_TAILLE_PAYLOAD_MAX ? payload_len : BROKER_TAILLE_PAYLOAD_MAX);
    s_retain[slot].payload_len = payload_len < BROKER_TAILLE_PAYLOAD_MAX ? payload_len : BROKER_TAILLE_PAYLOAD_MAX;
    s_retain[slot].qos = qos;

    ESP_LOGD(TAG, "Retain stocké : %s (%d octets)", topic, payload_len);
}

static void envoyer_retain_pour_client(int client_idx)
{
    for (int r = 0; r < BROKER_MAX_RETAIN; r++) {
        if (!s_retain[r].actif) continue;

        /* Vérifier si au moins une souscription de ce client matche */
        for (int s = 0; s < BROKER_MAX_SOUSCRIPTIONS; s++) {
            if (s_souscriptions[s].client_idx != client_idx) continue;

            if (topic_correspond(s_souscriptions[s].filtre, s_retain[r].topic)) {
                uint8_t qos_eff = s_retain[r].qos < s_souscriptions[s].qos
                                  ? s_retain[r].qos : s_souscriptions[s].qos;
                uint16_t pid = (qos_eff >= 1) ? s_next_packet_id++ : 0;
                if (s_next_packet_id == 0) s_next_packet_id = 1;

                envoyer_publish(s_clients[client_idx].sock, s_retain[r].topic,
                                s_retain[r].payload, s_retain[r].payload_len, qos_eff, pid);
                break;  /* Ne pas envoyer 2 fois le même retain */
            }
        }
    }
}

/* ====================================================================
 * TÂCHE PRINCIPALE DU BROKER
 * ==================================================================== */

static void broker_tache(void *param)
{
    ESP_LOGI(TAG, "Tâche broker démarrée.");

    while (s_actif) {
        fd_set read_fds;
        FD_ZERO(&read_fds);

        int max_fd = s_socket_ecoute;
        FD_SET(s_socket_ecoute, &read_fds);

        /* Ajouter tous les sockets clients */
        for (int i = 0; i < BROKER_MAX_CLIENTS; i++) {
            if (s_clients[i].sock >= 0) {
                FD_SET(s_clients[i].sock, &read_fds);
                if (s_clients[i].sock > max_fd) max_fd = s_clients[i].sock;
            }
        }

        struct timeval tv = { .tv_sec = 0, .tv_usec = 100000 };  /* 100ms */
        int ret = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            ESP_LOGE(TAG, "Erreur select : %d", errno);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (ret == 0) continue;  /* Timeout */

        /* Nouvelle connexion ? */
        if (FD_ISSET(s_socket_ecoute, &read_fds)) {
            struct sockaddr_in addr_client;
            socklen_t addr_len = sizeof(addr_client);
            int new_sock = accept(s_socket_ecoute, (struct sockaddr *)&addr_client, &addr_len);

            if (new_sock >= 0) {
                xSemaphoreTake(s_mutex, portMAX_DELAY);
                int idx = client_trouver_libre();
                if (idx >= 0) {
                    s_clients[idx].sock = new_sock;
                    s_clients[idx].connecte = false;
                    s_clients[idx].buf_len = 0;
                    s_clients[idx].dernier_paquet_ms = esp_timer_get_time() / 1000;
                    s_clients[idx].keepalive_s = 0;
                }
                xSemaphoreGive(s_mutex);

                if (idx >= 0) {
                    int flag = 1;
                    lwip_setsockopt(new_sock, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag));
                    ESP_LOGI(TAG, "Nouveau client [%d] depuis %s:%d",
                             idx, inet_ntoa(addr_client.sin_addr), ntohs(addr_client.sin_port));
                } else {
                    ESP_LOGW(TAG, "Max clients atteint, connexion refusée.");
                    close(new_sock);
                }
            }
        }

        /* Vérification keepalive : déconnecter les clients silencieux */
        {
            int64_t now_ms = esp_timer_get_time() / 1000;
            for (int i = 0; i < BROKER_MAX_CLIENTS; i++) {
                if (s_clients[i].sock < 0 || !s_clients[i].connecte) continue;
                if (s_clients[i].keepalive_s == 0) continue;
                int64_t timeout_ms = (int64_t)s_clients[i].keepalive_s * 1500;
                if (now_ms - s_clients[i].dernier_paquet_ms > timeout_ms) {
                    ESP_LOGW(TAG, "Client [%d] '%s' keepalive expiré, déconnexion.",
                             i, s_clients[i].client_id);
                    client_deconnecter(i);
                }
            }
        }

        /* Données des clients existants */
        for (int i = 0; i < BROKER_MAX_CLIENTS; i++) {
            if (s_clients[i].sock < 0) continue;
            if (!FD_ISSET(s_clients[i].sock, &read_fds)) continue;

            int espace = BROKER_TAILLE_BUFFER - s_clients[i].buf_len;
            if (espace <= 0) {
                ESP_LOGW(TAG, "Buffer client [%d] plein, déconnexion.", i);
                client_deconnecter(i);
                continue;
            }

            int n = recv(s_clients[i].sock,
                         s_clients[i].buffer + s_clients[i].buf_len,
                         espace, 0);

            if (n <= 0) {
                /* Déconnexion ou erreur */
                client_deconnecter(i);
                continue;
            }

            s_clients[i].buf_len += n;

            /* Traiter autant de paquets complets que possible dans le buffer */
            while (s_clients[i].buf_len > 0 && s_clients[i].sock >= 0) {
                if (s_clients[i].buf_len < 2) break;  /* Minimum : type + longueur */

                int octets_rl;
                int remaining = paquet_lire_longueur_restante(
                    s_clients[i].buffer + 1,
                    s_clients[i].buf_len - 1,
                    &octets_rl);

                if (remaining < 0) break;  /* Pas encore assez de données pour la longueur */

                int longueur_totale = 1 + octets_rl + remaining;
                if (s_clients[i].buf_len < longueur_totale) break;  /* Paquet incomplet */

                /* Paquet complet → traiter */
                traiter_paquet(i, s_clients[i].buffer, longueur_totale);

                /* Décaler le buffer (retirer le paquet traité) */
                int restant = s_clients[i].buf_len - longueur_totale;
                if (restant > 0) {
                    memmove(s_clients[i].buffer, s_clients[i].buffer + longueur_totale, restant);
                }
                s_clients[i].buf_len = restant;
            }
        }
    }

    /* Nettoyage */
    for (int i = 0; i < BROKER_MAX_CLIENTS; i++) {
        if (s_clients[i].sock >= 0) client_deconnecter(i);
    }
    if (s_socket_ecoute >= 0) {
        close(s_socket_ecoute);
        s_socket_ecoute = -1;
    }

    ESP_LOGI(TAG, "Tâche broker arrêtée.");
    s_tache_handle = NULL;
    vTaskDelete(NULL);
}

/* ====================================================================
 * FONCTIONS PUBLIQUES
 * ==================================================================== */

esp_err_t broker_mqtt_demarrer(const broker_mqtt_config_t *config)
{
    if (s_actif) {
        ESP_LOGW(TAG, "Broker déjà actif.");
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t port = config ? config->port : BROKER_PORT_DEFAUT;

    /* Initialiser les structures */
    for (int i = 0; i < BROKER_MAX_CLIENTS; i++) {
        s_clients[i].sock = -1;
        s_clients[i].connecte = false;
        s_clients[i].buf_len = 0;
    }
    for (int i = 0; i < BROKER_MAX_SOUSCRIPTIONS; i++) {
        s_souscriptions[i].client_idx = -1;
    }
    for (int i = 0; i < BROKER_MAX_RETAIN; i++) {
        s_retain[i].actif = false;
    }

    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
    }

    /* Créer le socket d'écoute */
    s_socket_ecoute = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s_socket_ecoute < 0) {
        ESP_LOGE(TAG, "Erreur création socket : %d", errno);
        return ESP_FAIL;
    }

    /* SO_REUSEADDR pour redémarrage rapide */
    int opt = 1;
    setsockopt(s_socket_ecoute, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(s_socket_ecoute, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Erreur bind port %d : %d", port, errno);
        close(s_socket_ecoute);
        s_socket_ecoute = -1;
        return ESP_FAIL;
    }

    if (listen(s_socket_ecoute, BROKER_MAX_CLIENTS) < 0) {
        ESP_LOGE(TAG, "Erreur listen : %d", errno);
        close(s_socket_ecoute);
        s_socket_ecoute = -1;
        return ESP_FAIL;
    }

    s_actif = true;

    /* Lancer la tâche */
    BaseType_t res = xTaskCreate(broker_tache, "mqtt_broker",
                                 BROKER_TASK_STACK, NULL, BROKER_TASK_PRIORITE,
                                 &s_tache_handle);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Erreur création tâche broker.");
        close(s_socket_ecoute);
        s_socket_ecoute = -1;
        s_actif = false;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "=== BROKER MQTT démarré sur port %d ===", port);
    return ESP_OK;
}

esp_err_t broker_mqtt_arreter(void)
{
    if (!s_actif) return ESP_OK;

    ESP_LOGI(TAG, "Arrêt du broker...");
    s_actif = false;

    /* La tâche se terminera d'elle-même lors du prochain cycle */
    /* Attendre un peu que la tâche se termine */
    for (int i = 0; i < 20 && s_tache_handle != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    return ESP_OK;
}

bool broker_mqtt_est_actif(void)
{
    return s_actif;
}

int broker_mqtt_nb_clients(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int nb = 0;
    for (int i = 0; i < BROKER_MAX_CLIENTS; i++) {
        if (s_clients[i].sock >= 0 && s_clients[i].connecte) nb++;
    }
    xSemaphoreGive(s_mutex);
    return nb;
}
