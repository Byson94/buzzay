# Xwayland

Xwayland allows legacy X11 applications to run on wayland. Buzzay as of right now **does not** have plans for implementing xwayland support.
Instead, users should use something called `xwayland-satellite` if they want xwayland support. This page will document how to setup
xwayland-satellite on buzzay.

## Installation

`xwayland-satellite` is a pretty known program that should be available in your linux distro's package manager. Just install it from there.
If you are **not** installing it from your package manager, then make sure that it is installed to `/usr/bin/xwayland-satellite`.

## Setup

If `xwayland-satellite` is correctly installed, it should be accissible at `/usr/bin/xwayland-satellite`. You can check it by running 
this command:

```bash
$ which xwayland-satellite
```

That's it. Buzzay will automatically start it up and set `DISPLAY=:0` environment variable.
