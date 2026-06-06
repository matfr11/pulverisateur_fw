CJSON_DIR   = /mnt/c/Users/destr/esp/v5.5.2/esp-idf/components/json/cJSON
CJSON       = vendor/cJSON.c

INC = -I. \
      -Imocks \
      -I../commun/include \
      -I../commun/broker_mqtt \
      -I../commun/protocoles \
      -I../carte_relais/main \
      -I../carte_relais/components/gestion_securites/include \
      -I../carte_relais/components/gestion_automatismes/include \
      -I../carte_relais/components/gestion_actionneurs/include \
      -I../carte_relais/components/gestion_capteurs/include \
      -I$(CJSON_DIR)

DEFS = -DCARTE_AVANT \
       -DA_DEBITMETRE=1 \
       -DA_SONDE_NIVEAU=0 \
       -DA_WEB_UI=0 \
       -DA_EST_SERVEUR=0 \
       -DA_VANNES_MOTORISEES=0 \
       -DMODE_SIMULATION

CFLAGS = -Wall -Wextra -Wno-unused-function -Wno-unused-parameter \
         -g -O0 $(INC) $(DEFS)

.PHONY: all run clean

all: test_broker test_json test_securites test_automatismes test_ota_proxy

run: all
	@echo ""
	@echo "\033[1;37m━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\033[0m"
	@echo "\033[1;37m  LANCEMENT DES TESTS\033[0m"
	@echo "\033[1;37m━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\033[0m"
	@./test_broker    ; echo ""
	@./test_json      ; echo ""
	@./test_securites ; echo ""
	@./test_automatismes ; echo ""
	@./test_ota_proxy

test_broker: test_broker_logic.c stubs/broker_stubs.c
	@echo "[CC] test_broker"
	@gcc $(CFLAGS) -o $@ $^

test_json: test_json.c $(CJSON)
	@echo "[CC] test_json"
	@gcc $(CFLAGS) -I$(CJSON_DIR) -o $@ $^ -lm

test_securites: test_securites.c
	@echo "[CC] test_securites"
	@gcc $(CFLAGS) -o $@ $^

test_automatismes: test_automatismes.c
	@echo "[CC] test_automatismes"
	@gcc $(CFLAGS) -o $@ $^

test_ota_proxy: test_ota_proxy.c
	@echo "[CC] test_ota_proxy"
	@gcc $(CFLAGS) -I../carte_relais/components/gestion_ota/include -o $@ $^

clean:
	rm -f test_broker test_json test_securites test_automatismes test_ota_proxy
