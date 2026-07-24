# Getting Started

If you installed buzzay from your linux distribution's package manager, then you should have access to the plugin
API directory which can be included with `#include <buzzay-plugin.h>`. If it is not, then make sure to 
grab the `buzzay-plugin.h` file from the **include/** directory in the source code and use it.

```{code-block} c
:caption: Basic Setup

#include <stdio.h>
#include <buzzay-plugin.h>

int buzzay_api_version() {
    return BUZZAY_API_VERSION;
}

void msg_request(struct bz_plugin *plugin, int client_fd, int argc, char **argv) {
    write_ipc_response("OK!\n");
}

void init_plugin(struct bz_plugin *plugin) {
    printf("Plugin initialized!\n");
}
```

This is the most basic plugin. Here is what is happening:

- `buzzay_api_version` This is required to expose the API_VERSION of the library.
- `msg_request` This function is called when user runs `buzzay --msg your_plugin`.
- `init_plugin` This function is called to initialize the plugin.

You will mostly write your code inside the `msg_request` and `init_plugin` functions. You can now 
move on and take a look at the API's that `buzzay-plugin.h` provides.
