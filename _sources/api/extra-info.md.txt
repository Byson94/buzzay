# Extra Information

Extra information about plugins are handled.

## Plugin Resolval

For a plugin to be resolved, they must be in the `/usr/lib/buzzay-plugins/` directory root.
The name of the plugin is learnt directly from the filename instead of any metadata provided 
inside the plugin.

## Plugin Compilation

It is the duty of the plugin author to ensure that the plugin is compatible with buzzay.
So the plugins must link to the same wlroots version that buzzay is linked to. Moreover, 
they should ensure that the plugin is also compiled against the latest version of `buzzay-plugin.h`.

## Breaking Changes

I plan on avoiding breaking changes as much as possible. Especially in the plugin api library (`buzzay-plugin.h`).
Using the API abstractions provided in the library is much safer than directly working with `buzzay_server` as
it has the lowest changes of encountering changes.
