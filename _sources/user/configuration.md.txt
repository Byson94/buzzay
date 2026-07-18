# Configuration

Buzzay is configured through the IPC in a bash file located at `$XDG_CONFIG_HOME/buzzay/init`. On most systems,
this will be the `~/.config/buzzay/init` file.

## Basics

Inside the the init file, you can call back to Buzzay IPC using the `buzzay` command.

```{code-block} bash
:caption: ~/.config/buzzay/init

#!/bin/bash

# Load initial plugins
buzzay --load coreconf
buzzay --load kbinder
buzzay --load wmpire

# Setup keybindings
buzzay --msg kbinder "Super+Enter" "kitty"
```

And that's pretty much it! You will mostly be loading plugins and sending messages to them.

```{tip}
You will mostly have to do a lot of `buzzay --msg` calls in your configuration. So it is recommended to 
create a alias to that command. Try something like `bzmsg`!
```
