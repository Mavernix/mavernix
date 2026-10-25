# Dragging the edge two tiled windows share makes one grow as the other shrinks.

# Wayfire looks for lib<name>.so, so the name must match the plugin's
# name in metadata/shared-edges.xml.
shared_module(
    'shared-edges',
    ['shared-edges.cpp', 'seam-math.cpp'],
    dependencies: [wayfire],
    cpp_args: wayfire_plugin_args,
    install: true,
    install_dir: wayfire_plugin_dir,
)
install_data('metadata/shared-edges.xml', install_dir: wayfire_metadata_dir)
