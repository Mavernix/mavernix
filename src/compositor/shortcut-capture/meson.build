# Lets the settings window record a shortcut by having the keys pressed.

# Wayfire looks for lib<name>.so, so the name must match the plugin's
# name in metadata/shortcut-capture.xml.
shared_module(
    'shortcut-capture',
    ['shortcut-capture.cpp', 'capture-holder.cpp'],
    dependencies: [wayfire],
    cpp_args: wayfire_plugin_args,
    install: true,
    install_dir: wayfire_plugin_dir,
)
install_data('metadata/shortcut-capture.xml', install_dir: wayfire_metadata_dir)
