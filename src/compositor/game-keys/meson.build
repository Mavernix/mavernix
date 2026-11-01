# Lets the function keys of the desktop's shortcuts through to games.

# Wayfire looks for lib<name>.so, so the name must match the plugin's
# name in metadata/game-keys.xml.
shared_module(
    'game-keys',
    ['game-keys.cpp'],
    dependencies: [wayfire],
    cpp_args: wayfire_plugin_args,
    install: true,
    install_dir: wayfire_plugin_dir,
)
install_data('metadata/game-keys.xml', install_dir: wayfire_metadata_dir)
