# Arrow keys that put the front window on a half, a quarter or the whole screen.

# Wayfire looks for lib<name>.so, so the name must match the plugin's
# name in metadata/snap-keys.xml.
shared_module(
    'snap-keys',
    ['snap-keys.cpp', 'snap-steps.cpp'],
    dependencies: [wayfire],
    cpp_args: wayfire_plugin_args,
    install: true,
    install_dir: wayfire_plugin_dir,
)
install_data('metadata/snap-keys.xml', install_dir: wayfire_metadata_dir)
