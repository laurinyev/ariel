LDRM_FLAGS = $(shell pkg-config --cflags --libs libdrm)
SDL2_CFLAGS = $(shell sdl2-config --cflags)
SDL2_LFLAGS  = $(shell sdl2-config --libs)

INCLUDE_PATHS = -I../libbetterm -I../libdazzle -I../libdazzletype -I../utils

.PHONY: all drmtest fb0test sdltest

all: drmtest fb0test sdltest

drmtest:
	gcc -o drmtest drmtest.c $(INCLUDE_PATHS) $(LDRM_FLAGS) -lm -g

fb0test:
	gcc -o fb0test fb0test.c $(INCLUDE_PATHS) -lm -g

sdltest:
	gcc -o sdltest sdltest.c $(INCLUDE_PATHS) $(SDL2_CFLAGS) $(SDL2_LFLAGS) -lm -g

run-drm: drmtest
	./drmtest

run-fb0: fb0test
	./fb0test

run-sdl: sdltest
	./sdltest
