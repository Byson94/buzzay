# Monitor Options

All the monitor configuration options in `[[monitor]]`.

## id

The id of the monitor to match. The id depends on your monitor and on a laptop, it is "eDP-1".

**Example:**

```toml
[[monitor]]
id = "eDP-1"
```

## enabled

Whether this monitor is enabled or not.

**Example:**

```toml
[[monitor]]
id = "eDP-1"
enabled = true
```

## transform

The transformation to apply on this monitor. Available options:

- `"normal"`: Default monitor rotation.
- `"90"`: Rotate 90 degrees.
- `"180"`: Rotate 180 degrees.
- `"270"`: Rotate 270 degrees.
- `"flipped"`: Flip the monitor.
- `"flipped-90"`: Flip the monitor and rotate it 90 degrees.
- `"flipped-180"`: Flip the monitor and rotate it 180 degrees.
- `"flipped-270"`: Flip the monitor and rotate it 270 degrees.

**Example:**

```toml
[[monitor]]
id = "eDP-1"
transform = "normal"
```

## mode 

Monitor mode in `{width}x{height}` or `{width}x{height}@{refresh}` format. Buzzay will pick the mode
closest to the one you provided.

**Example:**

```toml
[[monitor]]
id = "eDP-1"
mode = "1920x1080@120"
```

## scale

The scale of the monitor. Must be a floating number (0.0 - 1.0).

**Example:**

```toml
[[monitor]]
id = "eDP-1"
scale = 0.9
```

## position

The position at which the monitor should be in `[x, y]` format.

**Example:**

```toml
[[monitor]]
id = "eDP-1"
position = [0, 0]
```
