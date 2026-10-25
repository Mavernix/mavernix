# Keeps chosen parts of the shell in place while the desktops slide.

# Wayfire looks for lib<name>.so, so the name must match the plugin's
# name in metadata/still-surfaces.xml.
shared_module(
    'still-surfaces',
    ['still-surfaces.cpp'],
    dependencies: [wayfire],
    cpp_args: wayfire_plugin_args,
    install: true,
    install_dir: wayfire_plugin_dir,
)
install_data('metadata/still-surfaces.xml', install_dir: wayfire_metadata_dir)
