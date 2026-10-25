# The shell: panel, dock and overlays. Its QML is read by Quickshell at run
# time, so it is installed as it is; only the native module is compiled.

subdir('native')

install_subdir(
    '.',
    install_dir: mavernix_datadir / 'shell',
    strip_directory: true,
    exclude_directories: ['native'],
    exclude_files: ['meson.build'],
)
