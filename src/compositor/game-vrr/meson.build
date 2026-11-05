# Variable refresh rate on a screen only while a full-screen window shows.

# Wayfire looks for lib<name>.so, so the name must match the plugin's
# name in metadata/game-vrr.xml.
shared_module(
    'game-vrr',
    ['game-vrr.cpp'],
    dependencies: [wayfire],
    cpp_args: wayfire_plugin_args,
    install: true,
    install_dir: wayfire_plugin_dir,
)
install_data('metadata/game-vrr.xml', install_dir: wayfire_metadata_dir)
