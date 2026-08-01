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

# == Cursor ==

[cursor]
# set your cursor theme here
theme = "capitaine-cursors"
# set your cursor size here
size = 24

# == Eye Candy and Effects ==

[candy]
# gap to keep inbetween windows
gap = 2
# opacity of each window (0.0 - 1.0)
opacity = 1.0
# how much to curve a window
corner-radius = 5

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
# num of blur passes
passes = 3
# noise of the blur (glassy effect) (0.0 - 1.0)
noise = 0.0

# == Keybindings ==

[bindings]
"Super+Return" = ["spawn", "kitty"]
"Super+Q" = ["close-active-window"]

# Switch workspace (max 9 workspaces)
"Super+1" = ["switch-workspace", "1"]
"Super+2" = ["switch-workspace", "2"]
"Super+3" = ["switch-workspace", "3"]
"Super+4" = ["switch-workspace", "4"]
"Super+5" = ["switch-workspace", "5"]
"Super+6" = ["switch-workspace", "6"]
"Super+7" = ["switch-workspace", "7"]
"Super+8" = ["switch-workspace", "8"]
"Super+9" = ["switch-workspace", "9"]

# Move window to workspace (max 9 workspaces)
"Super+Shift+1" = ["window-to-workspace", "1"]
"Super+Shift+2" = ["window-to-workspace", "2"]
"Super+Shift+3" = ["window-to-workspace", "3"]
"Super+Shift+4" = ["window-to-workspace", "4"]
"Super+Shift+5" = ["window-to-workspace", "5"]
"Super+Shift+6" = ["window-to-workspace", "6"]
"Super+Shift+7" = ["window-to-workspace", "7"]
"Super+Shift+8" = ["window-to-workspace", "8"]
"Super+Shift+9" = ["window-to-workspace", "9"]

# Monocle layout
"Super+M" = ["toggle-monocle"]
"Super+J" = ["cycle-monocle"]

# Focusing Windows
"Super+Right" = ["focus-window", "right"]
"Super+Left" = ["focus-window", "left"]
"Super+Up" = ["focus-window", "up"]
"Super+Down" = ["focus-window", "down"]

# Moving windows
"Super+Shift+Right" = ["move-window", "right"]
"Super+Shift+Left" = ["move-window", "left"]
"Super+Shift+Up" = ["move-window", "up"]
"Super+Shift+Down" = ["move-window", "down"]

# Exit the compositor
"Super+Escape" = ["quit-compositor"]
```

