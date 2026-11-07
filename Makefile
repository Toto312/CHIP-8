CC ?= gcc

FLAGS ?= -Wall -Wextra -std=c99 -O2

# Ruta a la carpeta de Raylib (en Escritorio/Programacion/Librerias)
RAYLIB_DIR ?= $(HOME)/Escritorio/Programacion/Librerias/raylib-6.0_linux_amd64

# Si existe la carpeta local de Raylib, usamos sus include y lib; si no, buscamos en /usr/local o pkg-config
ifneq ($(wildcard $(RAYLIB_DIR)),)
    CFLAGS  += -I$(RAYLIB_DIR)/include
    LDFLAGS += -L$(RAYLIB_DIR)/lib -Wl,-rpath,$(RAYLIB_DIR)/lib
else ifeq ($(shell pkg-config --exists raylib && echo yes),yes)
    CFLAGS  += $(shell pkg-config --cflags raylib)
    LDFLAGS += $(shell pkg-config --libs-only-L raylib)
else
    CFLAGS  += -I/usr/local/include
    LDFLAGS += -L/usr/local/lib -Wl,-rpath,/usr/local/lib
endif

LDLIBS ?= -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

TARGET = chip8
SRCS   = main.c
OBJS   = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean run
