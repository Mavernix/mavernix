# One line in the journal when a program with a window loses its connection.

# Wayfire looks for lib<name>.so, so the name must match the plugin's
# name in metadata/closed-programs.xml.
shared_module(
    'closed-programs',
    ['closed-programs.cpp'],
    dependencies: [wayfire],
    cpp_args: wayfire_plugin_args,
    install: true,
    install_dir: wayfire_plugin_dir,
)
install_data('metadata/closed-programs.xml', install_dir: wayfire_metadata_dir)
