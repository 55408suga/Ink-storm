CC = gcc
CFLAGS ?= -O2 -DNDEBUG
RAYLIB_DIR ?= $(HOME)/.local/opt/raylib-6.2
GL_ENV ?= $(if $(wildcard /dev/dxg),GALLIUM_DRIVER=d3d12,)

MODULES := core fluid ecs game audio render app
MODULE_INCLUDES := $(addprefix -Isrc/,$(MODULES))
SOURCES := $(shell find src -type f -name '*.c' | sort)
OBJECTS := $(patsubst src/%.c,build/obj/%.o,$(SOURCES))

.PHONY: all run clean
all: inkstorm

inkstorm: $(OBJECTS)
	$(CC) $(LDFLAGS) -o "$@" $(OBJECTS) "$(RAYLIB_DIR)/lib/libraylib.a" -lGL -lm -lpthread -ldl -lrt -lX11 $(LDLIBS)

build/obj/render/%.o build/obj/app/%.o: RAYLIB_INCLUDE = -isystem "$(RAYLIB_DIR)/include"
SOLVER_VECT_FLAG := $(if $(findstring gcc,$(notdir $(CC))),-fvect-cost-model=cheap,)
build/obj/fluid/solver.o: EXTRA_CFLAGS = $(SOLVER_VECT_FLAG)

build/obj/%.o: src/%.c Makefile
	mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) -D_DEFAULT_SOURCE $(MODULE_INCLUDES) $(RAYLIB_INCLUDE) \
	    $(CFLAGS) $(EXTRA_CFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Wshadow -MMD -MP -c "$<" -o "$@"

run: inkstorm
	$(GL_ENV) ./inkstorm

clean:
	rm -rf build/obj inkstorm

-include $(OBJECTS:.o=.d)
