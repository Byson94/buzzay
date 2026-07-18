CC=gcc
PKG_CONFIG?=pkg-config

PKGS="wlroots-0.20" wayland-server xkbcommon
CFLAGS_PKG_CONFIG!=$(PKG_CONFIG) --cflags $(PKGS)
CFLAGS+=$(CFLAGS_PKG_CONFIG) -g -Werror -Iinclude -DWLR_USE_UNSTABLE -fvisibility=hidden
LDFLAGS += -rdynamic
LIBS!=$(PKG_CONFIG) --libs $(PKGS)

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c, build/%.o, $(SRCS))

all: build/buzzay

build/buzzay: $(OBJS)
	@mkdir -p build
	$(CC) $(OBJS) $(LDFLAGS) $(LIBS) -o $@

build/%.o: src/%.c
	@mkdir -p build
	$(CC) -c $< $(CFLAGS) -o $@

clean:
	rm -rf build

.PHONY: all clean

