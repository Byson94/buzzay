# Extra Information

Extra information about plugins are handled.

## Plugin Resolval

For a plugin to be resolved, they must be in the `/usr/lib/buzzay-plugins/` directory root or
the `~/.local/share/buzzay-plugins/` directory root.
The name of the plugin is learnt directly from the filename instead of any metadata provided 
inside the plugin.

## Plugin Compilation

Plugins should always aim to be up to date with the latest `buzzay-plugin.h`. Although buzzay
will let plugins using outdated versions of the header pass, it is always a good idea to be on
the safe side and use the latest API.

## Breaking Changes

I plan on avoiding breaking changes as much as possible. The existing function and property names 
in `buzzay-plugin.h` will also be preserved unless absolutely necessary. If a change has to be made,
then the previous version of the respective API will still be kept as a deprecated option that 
can be removed any time.
