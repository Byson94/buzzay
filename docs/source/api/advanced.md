# Advanced Plugins

Some plugins may find that the core abstraction is too limiting. Those plugins can drop down to the core compositor
server and work with it directly.

```c
#include <buzzay-plugin.h>
#include <buzzay-server.h>

// ...

void init_plugin(struct bz_plugin *plugin) {
    struct buzzay_server *server = bz_server_extract(plugin);
    // Do something with server...
}
```

This safely extracts the compositor server on which you can perform actions on.
