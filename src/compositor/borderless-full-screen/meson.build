# Makes a frameless X11 window that covers a whole screen full screen, as
# games do for "borderless full screen".

# Wayfire looks for lib<name>.so, so the name must match the plugin's
# name in metadata/borderless-full-screen.xml.
shared_module(
    'borderless-full-screen',
    ['borderless-full-screen.cpp'],
    dependencies: [wayfire],
    cpp_args: wayfire_plugin_args,
    install: true,
    install_dir: wayfire_plugin_dir,
)
install_data('metadata/borderless-full-screen.xml', install_dir: wayfire_metadata_dir)
