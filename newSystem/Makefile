# Makefile para photonbar
VERSION = 1.0
CC ?= g++

# --- CAMBIO 1: Obtener flags de PulseAudio de forma dinámica ---
PULSE_CFLAGS = $(shell pkg-config --cflags libpulse)
PULSE_LIBS = $(shell pkg-config --libs libpulse)

# --- CAMBIO 5: Obtener flags de curl y json-c ---
CURL_CFLAGS = $(shell pkg-config --cflags libcurl)
CURL_LIBS = $(shell pkg-config --libs libcurl)
JSON_CFLAGS = $(shell pkg-config --cflags json-c)
JSON_LIBS = $(shell pkg-config --libs json-c)

CFLAGS += -Wall -std=c99 -Os -DVERSION="\"$(VERSION)\"" -I/usr/include/freetype2 -DLEMONBAR_BUILDING
# --- CAMBIO 2: Añadido PULSE_CFLAGS a CXXFLAGS ---
CXXFLAGS += -Wall -std=c++11 -Os -DVERSION="\"$(VERSION)\"" -I/usr/include/freetype2 -DLEMONBAR_BUILDING -D_DEFAULT_SOURCE $(PULSE_CFLAGS) $(CURL_CFLAGS) $(JSON_CFLAGS)

# --- CAMBIO 3: Añadido PULSE_LIBS a LDFLAGS ---
LDFLAGS += -lxcb -lxcb-xinerama -lxcb-randr -lX11 -lX11-xcb -lXft -lfreetype -lz -lfontconfig -lfmt $(PULSE_LIBS) $(CURL_LIBS) $(JSON_LIBS)

# Configuración de debug
CFDEBUG = -g3 -pedantic -Wall -Wunused-parameter -Wlong-long \
          -Wsign-conversion -Wconversion -Wimplicit-function-declaration

# Ejecutable y archivos
EXEC = photonbar
SRCS = main.cpp process_manager.cpp bar.cpp
OBJS = build/main.o build/process_manager.o build/bar.o

# Dependencias de headers locales
HEADERS = bar.h modules/datetime.h modules/module.h process_manager.h

PREFIX ?= /usr/local
BINDIR = ${PREFIX}/bin

all: build ${EXEC}

# Crear directorio de build
build:
	mkdir -p build

# Compilación
build/main.o: main.cpp ${HEADERS}
	${CC} ${CXXFLAGS} -o $@ -c $<

build/process_manager.o: process_manager.cpp process_manager.h
	${CC} ${CXXFLAGS} -o $@ -c $<

build/bar.o: bar.cpp bar.h
	${CC} ${CXXFLAGS} -o $@ -c $<

${EXEC}: ${OBJS} ${HEADERS}
	g++ -o build/${EXEC} ${OBJS} ${LDFLAGS}

# Versión debug
debug: build ${EXEC}
debug: CC += ${CFDEBUG}
debug: CXXFLAGS += -g -O0

# Limpieza
clean:
	rm -rf build

# Instalación
install: ${EXEC}
	install -D -m 755 build/${EXEC} ${DESTDIR}${BINDIR}/${EXEC}

# Desinstalación
uninstall:
	rm -f ${DESTDIR}${BINDIR}/${EXEC}

# --- CAMBIO 4: Añadida verificación de PulseAudio ---
check-deps:
	@echo "Verificando dependencias..."
	@pkg-config --exists xcb || echo "ERROR: xcb no encontrado"
	@pkg-config --exists libpulse || echo "ERROR: libpulse no encontrado (instale libpulse-dev)"
	@pkg-config --exists xft || echo "ERROR: xft no encontrado"
	@pkg-config --exists fontconfig || echo "ERROR: fontconfig no encontrado"
	@pkg-config --exists libcurl || echo "ERROR: libcurl no encontrado (instale libcurl-dev)"
	@pkg-config --exists json-c || echo "ERROR: json-c no encontrado (instale json-c-dev)"
	@which i3-msg >/dev/null || echo "ADVERTENCIA: i3-msg no encontrado"

help:
	@echo "Targets disponibles: all, debug, clean, install, uninstall, check-deps, help"

.PHONY: all build debug clean install uninstall check-deps help
