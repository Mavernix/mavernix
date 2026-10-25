# Window frames drawn from the theme: title bar, buttons, corners and shadow.

# Wayfire looks for lib<name>.so, so the name must match the plugin's
# name in metadata/themed-decoration.xml.
shared_module(
    'themed-decoration',
    ['decoration.cpp', 'deco-subsurface.cpp', 'deco-button.cpp', 'deco-layout.cpp', 'deco-theme.cpp', 'theme-file.cpp', 'fill-painter.cpp', 'corner-clip.cpp'],
    dependencies: [wayfire, tomlplusplus],
    cpp_args: wayfire_plugin_args,
    install: true,
    install_dir: wayfire_plugin_dir,
)
install_data('metadata/themed-decoration.xml', install_dir: wayfire_metadata_dir)
