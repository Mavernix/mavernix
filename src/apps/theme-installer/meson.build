# The Theme Installer: a Qt Quick window over the import's back end, a few
# Python programs that read the installer and write the theme.

backend_install_dir = mavernix_libdir / 'theme-installer'
install_subdir('backend', install_dir: backend_install_dir, strip_directory: true)

moc_sources = qt.compile_moc(
    headers: ['import-job.hpp'],
    dependencies: qt_quick,
)

qml_install_dir = mavernix_datadir / 'apps' / 'theme-installer' / 'qml'
install_subdir('qml', install_dir: qml_install_dir, strip_directory: true)

executable(
    'mavernix-theme-installer',
    ['main.cpp', 'import-job.cpp', moc_sources],
    dependencies: [qt_quick],
    cpp_args: [
        # Where the program finds its QML files and the back end, as installed.
        '-DQML_DIRECTORY="' + (get_option('prefix') / qml_install_dir) + '"',
        '-DBACKEND_DIRECTORY="' + (get_option('prefix') / backend_install_dir) + '"',
    ],
    install: true,
    install_dir: mavernix_bindir,
)
