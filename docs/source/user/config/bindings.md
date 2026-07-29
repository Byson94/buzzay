# Keybinding Options

Keybindings are configured under the `[bindings]` table. They are in the provided format:

```toml
"Key1+Key2" = ["Action", "Argument"] # There can be multiple arguments.
```

All the elements in this array must always be a string.

## Modifier Keys

There are four modifier keys:

- `Super` or `Mod4`: The windows logo key.
- `Alt` or `Mod1`: The alt key.
- `Ctrl` or `Control`: The control key.
- `Shift`: The shift key.
- `ADPT`: Adaptive key that chooses between `Super` and `Alt` based on whether the compositor is running nested or not.

These keys can be paired with keys to get a keybinding. Example: `Super+Return`. The key names
are provided by xkbcommon, so you should follow those names for the keybinding to function. 
This is why `Return` is used instead of `Enter`. It is case insensitive, so something like `super+return` works too.

The paired keys can also be a keycode. They should be in the `code:<number>` format. Like this: `super+code:24`.
The keycode must be the raw scancode for the desired key used in the linux kernel.

## Actions

Each keybinding can peform an action, whether that be running a shell command, closing an application,
or switching the workspace. As shown above, they are an array like so:

```toml
kb = ["Action", "Argument"]
```

### spawn

This is the most common action. It is used to spawn a shell command. The shell command to execute
must be the argument.

**Example:**

```toml
[bindings]
# Format: keybinding = ["Action", "Argument"]
"Super+Return" = ["spawn", "kitty"]
```

### close-active-window

Close the currently active window. Takes no argument.

**Example:**

```toml
[bindings]
"Super+Q" = ["close-active-window"]
```

### focus-window

Focus a window. Takes the direction as an argument. Available directions:

- `"up"`
- `"down"`
- `"left"`
- `"right"`

**Example:**

```toml
[bindings]
"Super+Right" = ["focus-window", "right"]
```

### move-window

Move the focused window. Takes a direction as an argument (same options as `focus-window`).

**Example:**

```toml
[bindings]
"Super+Shift+Right" = ["move-window", "right"]
```

### window-to-workspace

Move the focused window to a workspace. Takes the workspace index to move to as the argument.

**Example:**

```toml
[bindings]
"Super+Shift+1" = ["window-to-workspace", 1]
```

### switch-workspace

Switch the currently active workspace. The argument should be the workspace number.

**Example:**

```toml
[bindings]
"Super+1" = ["switch-workspace", "1"]
```

### toggle-monocle

Toggle the monocle layout. Takes no argument.

**Example:**

```toml
[bindings]
"Super+M" = ["toggle-monocle"]
```

### cycle-monocle

Cycle to the next window in the monocle. Takes no argument.

**Example:**

```toml
[bindings]
"Super+J" = ["cycle-monocle"]
```

### quit-compositor

Quit the compositor. Takes no argument.

**Example:**

```toml
[bindings]
"Super+Escape" = ["quit-compositor"]
```
