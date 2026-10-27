# Tells the settings window the modes each screen can show.

# Wayfire looks for lib<name>.so, so the name must match the plugin's
# name in metadata/screen-modes.xml.
shared_module(
    'screen-modes',
    ['screen-modes.cpp'],
    dependencies: [wayfire],
    cpp_args: wayfire_plugin_args,
    install: true,
    install_dir: wayfire_plugin_dir,
)
install_data('metadata/screen-modes.xml', install_dir: wayfire_metadata_dir)
