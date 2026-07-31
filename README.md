<div align="center">
    <h1>Buzzay</h1>
    <p>An extensible & aesthetic wayland compositor</p>

[Documentaiton](https://byson94.is-a.dev/buzzay) • [Discord Server](https://discord.gg/UpRKtgkdM)
</div>

## About

Buzzay is a wayland compositor based on wlroots that is extensible and aesthetic. The compositor can be
extended to with IPC/Plugins. It supports eyecandy like blur, rounded corners, etc. along with
animations.

One of the other core philosophies of buzzay is stability. So we strictly adhere to Semantic Versioning 
and will be focused on improving the compositor without distrubing the end user.

To keep track of the progress, see the TODO list here: [Compositor TODO's](https://github.com/Byson94/buzzay/issues/1).

## Installation

Buzzay is currently only available in the AUR (Arch User Repository). You can install it with your favourite
AUR manager.

```bash
# With yay:
yay -S buzzay

# With paru:
paru -S buzzay
```

## Compile from Source

Install compile-time dependencies:

- meson
- wlroots
- wayland
- wayland-protocols
- pangocairo
- cairo
- uthash

Then run these commands:

```bash
meson setup build
ninja -C build
```

The compositor will be compiled to `./build/buzzay`.

## Configuration

See [docs/config](https://byson94.is-a.dev/buzzay/user/config/index.html) for documentation on configuring
buzzay.

## Acknowledgements

Buzzay started out by extending tinywl (CC0) from the wlroots team. The most of the eyecandy like blur, rounded
corners, opacity, etc. are made possible with the help of [scenefx](https://github.com/wlrfx/scenefx).
