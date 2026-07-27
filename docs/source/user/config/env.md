# Env Options

The `[env]` is a special table that can be used to setup environment variables. The environment variables are 
set at the start of the compositor, way before evaulation of the rest of the configuration options.

**Example:**

```toml
[env]
BROWSER = "firefox"
XDG_CONFIG_HOME = "/home/username/.config"
SOME_OTHER = "xyz"
```
