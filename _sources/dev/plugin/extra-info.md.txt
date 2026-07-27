# Extra Information

Extra information about plugins are handled.

## Plugin Resolval

For a plugin to be resolved, they must be in the `/usr/lib/buzzay-plugins/` directory root or
the `~/.local/share/buzzay-plugins/` directory root.
The name of the plugin is learnt directly from the filename instead of any metadata provided 
inside the plugin.

## Plugin Compilation

It is the duty of the plugin author to ensure that the plugin is compatible with buzzay.
So the plugins must link to the same wlroots version that buzzay is linked to. Moreover, 
they should ensure that the plugin is also compiled against the latest version of `buzzay-plugin.h`.

## Breaking Changes

I plan on avoiding breaking changes as much as possible. The existing function and property names 
in `buzzay-plugin.h` will also be preserved unless absolutely necessary. If a change has to be made,
then the previous version of the respective API will still be kept as a deprecated option that 
can be removed any time.
