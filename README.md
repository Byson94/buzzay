<div align="center">
    <h1>Buzzay</h1>
    <p>A plugin-first wayland compositor built on wlroots.</p>

[Documentaiton](https://byson94.is-a.dev/buzzay) • [Discord Server](https://discord.gg/UpRKtgkdM)
</div>

## About

Buzzay is a simple pugin-first wayland compositor that offloads all the work to plugins. This allows for an extensible
compositor where each component is optional.

## Download

Buzzay is currently under development and not yet available for download. But you can still compile it 
and test it out on your own!

To compile buzzay, just run the following commands.

```bash
$ meson setup build
$ ninja -C build
```

## Acknowledgements

Buzzay started out by extending tinywl (CC0) from the sway/wlroots team. This project wouldn't have been 
possible without the base of tinywl.
