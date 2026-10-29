# The App Store: a Qt Quick window over a Python back end that finds, installs
# and removes the apps an organisation publishes on GitHub.

backend_install_dir = mavernix_libdir / 'app-store'
install_subdir('backend', install_dir: backend_install_dir, strip_directory: true)

moc_sources = qt.compile_moc(
    headers: ['store.hpp'],
    dependencies: qt_quick,
)

qml_install_dir = mavernix_datadir / 'apps' / 'app-store' / 'qml'
install_subdir('qml', install_dir: qml_install_dir, strip_directory: true)

executable(
    'mavernix-app-store',
    ['main.cpp', 'store.cpp', moc_sources],
    dependencies: [qt_quick],
    cpp_args: [
        # Where the program finds its QML files and the back end, as installed.
        '-DQML_DIRECTORY="' + (get_option('prefix') / qml_install_dir) + '"',
        '-DBACKEND_DIRECTORY="' + (get_option('prefix') / backend_install_dir) + '"',
    ],
    install: true,
    install_dir: mavernix_bindir,
)
