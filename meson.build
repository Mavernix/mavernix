# Mavernix: a Wayland desktop session built on Wayfire and Quickshell.
#
#   meson setup build
#   meson compile -C build
#   meson test -C build
#   meson install -C build
#
# Everything is installed under PREFIX/lib/mavernix and PREFIX/share/mavernix,
# apart from the login screen's entry. docs/building.md lists what is needed.

project(
    'mavernix',
    'cpp',
    version: '0.5.4',
    license: 'GPL-3.0-or-later',
    meson_version: '>=1.3.0',
    default_options: [
        # The language level Wayfire itself is built with.
        'cpp_std=c++17',
        'warning_level=2',
        'buildtype=debugoptimized',
    ],
)

# Relative to the prefix; the session's scripts get the full paths
# (src/session/meson.build).
mavernix_libdir = get_option('libdir') / 'mavernix'
mavernix_datadir = get_option('datadir') / 'mavernix'

qt = import('qt6')
wayfire = dependency('wayfire', version: '>=0.11.0')
tomlplusplus = dependency('tomlplusplus')
gio = dependency('gio-unix-2.0')
librsvg = dependency('librsvg-2.0')
qt_quick = dependency('qt6', modules: ['Core', 'Gui', 'Qml', 'Quick'])
qt_widgets = dependency('qt6', modules: ['Core', 'Gui', 'Widgets', 'DBus', 'Network'])
qt_test = dependency('qt6', modules: ['Core', 'Gui', 'Test'])

subdir('src/shell')
subdir('src/compositor')
subdir('src/apps')
subdir('src/session')
subdir('data')
subdir('tests')
