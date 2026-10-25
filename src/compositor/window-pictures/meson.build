# Writes a small picture of one window into a file, for the dock's miniatures.

# Wayfire looks for lib<name>.so, so the name must match the plugin's
# name in metadata/window-pictures.xml.
shared_module(
    'window-pictures',
    ['window-pictures.cpp', 'shrink.cpp'],
    dependencies: [wayfire],
    cpp_args: wayfire_plugin_args,
    install: true,
    install_dir: wayfire_plugin_dir,
)
install_data('metadata/window-pictures.xml', install_dir: wayfire_metadata_dir)
