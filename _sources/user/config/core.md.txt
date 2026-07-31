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

## prefer-csd

Prefer client side decorations.

**Example:**

```toml
[core]
prefer-csd = true
```

## repeat-rate

The rate in which key should repeat. Default is `40`.

**Example:**

```toml
[core]
repeat-rate = 40
```

## repeat-delay

Delay in which key should repeat. Default is `600`.

```toml
[core]
repeat-delay = 600
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

## include

Include other configuration files. Must be an array like `spawn`.

**Example:**

```toml
[core]
include = ["other.toml"]
```
