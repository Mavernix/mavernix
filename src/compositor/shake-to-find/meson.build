# Shaking the pointer makes it big for a moment, so it's easy to find.

# Wayfire looks for lib<name>.so, so the name must match the plugin's
# name in metadata/shake-to-find.xml.
shared_module(
    'shake-to-find',
    ['shake-to-find.cpp', 'shake-detect.cpp'],
    dependencies: [wayfire],
    cpp_args: wayfire_plugin_args,
    install: true,
    install_dir: wayfire_plugin_dir,
)
install_data('metadata/shake-to-find.xml', install_dir: wayfire_metadata_dir)
