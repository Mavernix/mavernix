# Effects for minimising and restoring windows (genie and scale), added to Wayfire's animate plugin.

# Wayfire looks for lib<name>.so, so the name must match the plugin's
# name in metadata/themed-minimize.xml.
shared_module(
    'themed-minimize',
    ['minimize-effects.cpp', 'warp-transformer.cpp', 'warp-geometry.cpp'],
    dependencies: [wayfire],
    cpp_args: wayfire_plugin_args,
    install: true,
    install_dir: wayfire_plugin_dir,
)
install_data('metadata/themed-minimize.xml', install_dir: wayfire_metadata_dir)
