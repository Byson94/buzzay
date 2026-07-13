CC=gcc
PKG_CONFIG?=pkg-config

PKGS="wlroots-0.20" wayland-server xkbcommon
CFLAGS_PKG_CONFIG!=$(PKG_CONFIG) --cflags $(PKGS)
CFLAGS+=$(CFLAGS_PKG_CONFIG)
LIBS!=$(PKG_CONFIG) --libs $(PKGS)

all: build/buzzay

build/buzzay.o: src/*
	mkdir -p build
	$(CC) -c $< -g -Werror $(CFLAGS) -Iinclude -DWLR_USE_UNSTABLE -o $@

build/buzzay: build/buzzay.o
	$(CC) $^ -g -Werror $(CFLAGS) $(LDFLAGS) $(LIBS) -o $@

clean:
	rm -rf build

.PHONY: all clean

