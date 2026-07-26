# Core Options

All the configuration options under the `[core]` table.

## focus-on

How the window should be focused. Available options:

- `hover`: Focus the windows that is hovered.
- `click`: Focus the window that is clicked.

**Example:**

```toml
[core]
focus-on = "hover"
```

## layout-mode

What the default layout mode should be. Available options:

- `tiling`: The default layout will be tiling layout.
- `monocle`: The default layout will be monocle layout.

**Example:**

```toml
[core]
layout-mode = "tiling"
```

## spawn

The shell commands to run on startup. This option takes an array of strings that are executed when compositor loads.

**Example:**

```toml
[core]
spawn = [
    "kitty", # Spawn the kitty terminal
    "waybar" # Start waybar
]
```
