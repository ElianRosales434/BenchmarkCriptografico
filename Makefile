# ============================================================================
# UNIVERSIDAD DEL VALLE DE GUATEMALA - TRABAJO DE GRADUACIÓN
# Autor: Elián Israel Rosales Barrios
# Makefile para compilación nativa en Raspberry Pi OS y Android (Termux)
# ============================================================================

CC ?= gcc
CFLAGS ?= -O3 -Wall -Wextra -std=gnu11 -D_GNU_SOURCE -Iscripts/c
LDFLAGS ?= -lssl -lcrypto -lm

# Detección de entorno Termux o Linux estándar
ifdef PREFIX
    # Termux en Android
    CC = clang
    INCLUDES = -I$(PREFIX)/include -Iscripts/c
    LIBS = -L$(PREFIX)/lib -lssl -lcrypto -lm
    LIBOQS_DIR ?= $(PREFIX)
else
    # Raspberry Pi OS / Linux
    INCLUDES = -I/usr/local/include -Iscripts/c
    LIBS = -L/usr/local/lib -lssl -lcrypto -lm
    LIBOQS_DIR ?= /usr/local
endif

# Rutas de liboqs (se puede sobreescribir con: make LIBOQS_DIR=/ruta/a/liboqs/build)
OQS_INCLUDES = -I$(LIBOQS_DIR)/include
OQS_LIBS = -L$(LIBOQS_DIR)/lib -loqs

BIN_DIR = bin
SCRIPTS_C = scripts/c

TARGET_RSA = $(BIN_DIR)/rsa_benchmark
TARGET_KYBER = $(BIN_DIR)/kyber_benchmark
TARGET_ORQ = $(BIN_DIR)/orquestador
TARGET_INV = $(BIN_DIR)/inventario

.PHONY: all clean dirs

all: dirs $(TARGET_RSA) $(TARGET_KYBER) $(TARGET_ORQ) $(TARGET_INV)

dirs:
	mkdir -p $(BIN_DIR)
	mkdir -p resultados/piloto
	mkdir -p resultados/ejecucion_01
	mkdir -p resultados/ejecucion_02
	mkdir -p entorno

$(TARGET_RSA): $(SCRIPTS_C)/rsa_benchmark.c $(SCRIPTS_C)/common_bench.h
	@echo "==> Compilando benchmark RSA en C..."
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@ $(LIBS)
	@echo "[OK] Generado $@"

$(TARGET_KYBER): $(SCRIPTS_C)/kyber_benchmark.c $(SCRIPTS_C)/common_bench.h
	@echo "==> Compilando benchmark Kyber/ML-KEM en C..."
	$(CC) $(CFLAGS) $(INCLUDES) $(OQS_INCLUDES) $< -o $@ $(OQS_LIBS) $(LIBS)
	@echo "[OK] Generado $@"

$(TARGET_ORQ): $(SCRIPTS_C)/orquestador.c $(SCRIPTS_C)/common_bench.h
	@echo "==> Compilando orquestador nativo en C..."
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@ $(LIBS)
	@echo "[OK] Generado $@"

$(TARGET_INV): $(SCRIPTS_C)/inventario.c
	@echo "==> Compilando inventario nativo en C..."
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@ $(LIBS)
	@echo "[OK] Generado $@"

clean:
	rm -f $(TARGET_RSA) $(TARGET_KYBER) $(TARGET_ORQ) $(TARGET_INV)
