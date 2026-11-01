# Keeps every window's title bar out from under the menu bar.

# Wayfire looks for lib<name>.so, so the name must match the plugin's
# name in metadata/reachable-title-bars.xml.
shared_module(
    'reachable-title-bars',
    ['reachable-title-bars.cpp'],
    dependencies: [wayfire],
    cpp_args: wayfire_plugin_args,
    install: true,
    install_dir: wayfire_plugin_dir,
)
install_data('metadata/reachable-title-bars.xml', install_dir: wayfire_metadata_dir)
