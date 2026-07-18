CC=gcc
PKG_CONFIG?=pkg-config

PKGS="wlroots-0.20" wayland-server xkbcommon
CFLAGS_PKG_CONFIG!=$(PKG_CONFIG) --cflags $(PKGS)
CFLAGS+=$(CFLAGS_PKG_CONFIG) -g -Werror -Iinclude -Iprotocols -DWLR_USE_UNSTABLE -fvisibility=hidden
LDFLAGS += -rdynamic
LIBS!=$(PKG_CONFIG) --libs $(PKGS)

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c, build/%.o, $(SRCS))

PROTOCOLS := wlr-layer-shell-unstable-v1
PROTO_DIR := /usr/share/wlr-protocols/unstable

PROTO_HEADERS := $(patsubst %, protocols/%-protocol.h, $(PROTOCOLS))

all: build/buzzay

build/buzzay: $(OBJS)
	@mkdir -p build
	$(CC) $(OBJS) $(LDFLAGS) $(LIBS) -o $@

build/%.o: src/%.c $(PROTO_HEADERS)
	@mkdir -p build
	$(CC) -c $< $(CFLAGS) -o $@

protocols/%-protocol.h: $(PROTO_DIR)/%.xml
	@mkdir -p protocols
	wayland-scanner client-header $< $@

clean:
	rm -rf build protocols

.PHONY: all clean

