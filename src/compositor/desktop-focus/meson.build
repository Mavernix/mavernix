# Lets a click on the desktop leave no window active.

# Wayfire looks for lib<name>.so, so the name must match the plugin's
# name in metadata/desktop-focus.xml.
shared_module(
    'desktop-focus',
    ['desktop-focus.cpp'],
    dependencies: [wayfire],
    cpp_args: wayfire_plugin_args,
    install: true,
    install_dir: wayfire_plugin_dir,
)
install_data('metadata/desktop-focus.xml', install_dir: wayfire_metadata_dir)
