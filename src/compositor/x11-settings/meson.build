# Makes the main screen X11's primary screen, for games under Xwayland.

# Wayfire looks for lib<name>.so, so the name must match the plugin's
# name in metadata/x11-settings.xml.
shared_module(
    'x11-settings',
    ['x11-settings.cpp'],
    dependencies: [wayfire, dependency('xcb'), dependency('xcb-randr')],
    cpp_args: wayfire_plugin_args,
    install: true,
    install_dir: wayfire_plugin_dir,
)
install_data('metadata/x11-settings.xml', install_dir: wayfire_metadata_dir)
