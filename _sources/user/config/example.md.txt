## Example Config

Here is an example configuration for you to copy paste.

```toml
# == Core configs ==

[core]
# 'click' or 'hover'
focus-on = "hover" 
# default layout mode (tiling or moncole)
layout-mode = "tiling" 
# shell commands to spawn on startup
spawn = [
    "wlsunset -t 4000 -T 4001",
    "kitty"
]

# == Environment Variables ==

[env]
# set your cursor theme here
xcursor-theme = "capitaine-cursors"
# set your cursor size here
xcursor-size = 24

# == Eye Candy and Effects ==

[candy]
# gap to keep inbetween windows
gap = 2
# opacity of each window (0.0 - 1.0)
opacity = 1.0

[candy.border]
# How thick the window border should be
thickness = 2
# The border color of the focused window
active = "#eee"
# The border color of the non-focused windows
inactive = "#aaa"
 
[candy.blur]
# enable the blur effect (only visible if opacity is lower than 1.0)
enabled = true
# strength of the blur (0.0 - 1.0)
strength = 1.0
# alpha of the blur (0.0 - 1.0)
alpha = 1.0

# == Keybindings ==

[bindings]
"Super+Return" = ["spawn", "kitty"]
"Super+Q" = ["close-active-window"]

# Switch workspace (max 9 workspaces)
"Super+1" = ["switch-workspace", "1"]
"Super+2" = ["switch-workspace", "2"]
"Super+3" = ["switch-workspace", "3"]
"Super+4" = ["switch-workspace", "4"]

# Monocle layout
"Super+M" = ["toggle-monocle"]
"Super+J" = ["cycle-monocle"]

# Exit the compositor
"Super+Escape" = ["quit-compositor"]
```

