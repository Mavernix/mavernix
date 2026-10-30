# An effect for opening and closing windows ("pop"), added to Wayfire's animate plugin.

# Wayfire looks for lib<name>.so, so the name must match the plugin's
# name in metadata/themed-open-close.xml.
shared_module(
    'themed-open-close',
    ['open-close-effects.cpp'],
    dependencies: [wayfire],
    cpp_args: wayfire_plugin_args,
    install: true,
    install_dir: wayfire_plugin_dir,
)
install_data('metadata/themed-open-close.xml', install_dir: wayfire_metadata_dir)
