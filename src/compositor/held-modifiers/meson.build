# Tells the shell which modifier keys are held down, for the program switcher.

# Wayfire looks for lib<name>.so, so the name must match the plugin's
# name in metadata/held-modifiers.xml.
shared_module(
    'held-modifiers',
    ['held-modifiers.cpp'],
    dependencies: [wayfire],
    cpp_args: wayfire_plugin_args,
    install: true,
    install_dir: wayfire_plugin_dir,
)
install_data('metadata/held-modifiers.xml', install_dir: wayfire_metadata_dir)
