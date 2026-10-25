# The About This Computer window: a Qt Quick program. The compiled part only
# finds out what the machine is made of and starts the programs behind the
# window's buttons; its looks are qml/Main.qml.

moc_sources = qt.compile_moc(
    headers: ['machine.hpp', 'launcher.hpp'],
    dependencies: qt_quick,
)

qml_install_dir = mavernix_datadir / 'apps' / 'about' / 'qml'
install_subdir('qml', install_dir: qml_install_dir, strip_directory: true)

executable(
    'mavernix-about',
    ['main.cpp', 'machine.cpp', 'launcher.cpp', moc_sources],
    dependencies: [qt_quick],
    # Where the program finds its QML files, as installed.
    cpp_args: ['-DQML_DIRECTORY="' + (get_option('prefix') / qml_install_dir) + '"'],
    install: true,
    install_dir: mavernix_bindir,
)
