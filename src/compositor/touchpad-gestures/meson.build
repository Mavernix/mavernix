# Runs a command when three or four fingers swipe or pinch on a touchpad.

# Wayfire looks for lib<name>.so, so the name must match the plugin's
# name in metadata/touchpad-gestures.xml.
shared_module(
    'touchpad-gestures',
    ['touchpad-gestures.cpp', 'gesture-reading.cpp'],
    dependencies: [wayfire],
    cpp_args: wayfire_plugin_args,
    install: true,
    install_dir: wayfire_plugin_dir,
)
install_data('metadata/touchpad-gestures.xml', install_dir: wayfire_metadata_dir)
