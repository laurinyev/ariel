SDL2_CFLAGS = $(shell sdl2-config --cflags)
SDL2_LFLAGS  = $(shell sdl2-config --libs)

INCLUDE_PATHS = -I../libbetterm -I../libdazzle -I../libdazzletype -I../utils

.PHONY: all terminal-sdl

all: terminal-sdl

terminal-sdl:
	gcc -o terminal-sdl terminal_sdl.c $(INCLUDE_PATHS) $(SDL2_CFLAGS) $(SDL2_LFLAGS) -lm -g

run-terminal-sdl: terminal-sdl
	./terminal-sdl
