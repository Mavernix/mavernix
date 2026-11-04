# Draw window reflections on a dock shelf.

# Wayfire looks for lib<name>.so, so the name must match the plugin's
# name in metadata/dock-window-reflections.xml.
shared_module(
    'dock-window-reflections',
    ['reflections.cpp'],
    dependencies: [wayfire],
    cpp_args: wayfire_plugin_args,
    install: true,
    install_dir: wayfire_plugin_dir,
)
install_data('metadata/dock-window-reflections.xml', install_dir: wayfire_metadata_dir)
