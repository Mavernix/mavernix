# The shell's native QML module, "Shell.Native": the few things the shell
# needs that QML cannot do by itself.

# Private headers: icon-files.cpp uses Qt's internal icon lookup.
qt_native = dependency('qt6', modules: ['Core', 'DBus', 'Gui', 'Network', 'Qml', 'Quick'],
    private_headers: true)

moc_sources = qt.compile_moc(
    headers: [
        'active-window.hpp',
        'application-menu.hpp',
        'compositor-socket.hpp',
        'desktops.hpp',
        'file-kind.hpp',
        'folder-contents.hpp',
        'folder-watch.hpp',
        'held-keys.hpp',
        'icon-files.hpp',
        'lid-watch.hpp',
        'menu-registrar.hpp',
        'plugin.hpp',
        'session-watch.hpp',
        'sound-pack.hpp',
        'window-pictures.hpp',
    ],
    dependencies: qt_native,
)

module_dir = mavernix_libdir / 'qml' / 'Shell' / 'Native'

shared_module(
    'shellnative',
    [
        'active-window.cpp',
        'application-menu.cpp',
        'compositor-socket.cpp',
        'desktops.cpp',
        'file-kind.cpp',
        'folder-contents.cpp',
        'folder-watch.cpp',
        'held-keys.cpp',
        'icon-files.cpp',
        'lid-watch.cpp',
        'menu-registrar.cpp',
        'plugin.cpp',
        'session-watch.cpp',
        'sound-pack.cpp',
        'window-picture-provider.cpp',
        'window-pictures.cpp',
        moc_sources,
    ],
    dependencies: [qt_native, librsvg],
    install: true,
    install_dir: module_dir,
)

install_data('qmldir', install_dir: module_dir)
