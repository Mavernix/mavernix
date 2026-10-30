# Tells the shell when a window asks for attention, for the dock's bounce.

# Wayfire looks for lib<name>.so, so the name must match the plugin's
# name in metadata/window-attention.xml.
shared_module(
    'window-attention',
    ['window-attention.cpp'],
    dependencies: [wayfire],
    cpp_args: wayfire_plugin_args,
    install: true,
    install_dir: wayfire_plugin_dir,
)
install_data('metadata/window-attention.xml', install_dir: wayfire_metadata_dir)
