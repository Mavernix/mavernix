# Slides the desktop aside to uncover the list of notifications under it.

# Wayfire looks for lib<name>.so, so the name must match the plugin's
# name in metadata/desktop-aside.xml.
shared_module(
    'desktop-aside',
    ['desktop-aside.cpp'],
    dependencies: [wayfire],
    cpp_args: wayfire_plugin_args,
    install: true,
    install_dir: wayfire_plugin_dir,
)
install_data('metadata/desktop-aside.xml', install_dir: wayfire_metadata_dir)
