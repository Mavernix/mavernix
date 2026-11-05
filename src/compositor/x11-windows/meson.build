# Makes a frameless X11 window that covers a whole screen full screen, as
# games do for "borderless full screen".

# Wayfire looks for lib<name>.so, so the name must match the plugin's
# name in metadata/x11-windows.xml.
shared_module(
    'x11-windows',
    ['x11-windows.cpp'],
    dependencies: [wayfire],
    cpp_args: wayfire_plugin_args,
    install: true,
    install_dir: wayfire_plugin_dir,
)
install_data('metadata/x11-windows.xml', install_dir: wayfire_metadata_dir)
