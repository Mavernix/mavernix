# What the desktop reads rather than runs: the desktop configs, the themes
# and the keys of the theme format. Installed into PREFIX/share/mavernix.

install_subdir('config', install_dir: mavernix_datadir)
install_subdir('themes', install_dir: mavernix_datadir)
subdir('icons')

# The keys the theme documents name, which check_theme.py holds themes to.
theme_docs_dir = meson.project_source_root() / 'docs' / 'themes'
custom_target(
    'theme-keys',
    input: files('../src/session/check_theme.py'),
    output: 'theme-keys.json',
    command: [find_program('python3'), '@INPUT@', '--collect', theme_docs_dir, '@OUTPUT@'],
    depend_files: files(
        '../docs/themes/README.md',
        '../docs/themes/desktop.md',
        '../docs/themes/dock.md',
        '../docs/themes/menus-and-dialogs.md',
        '../docs/themes/other-applications.md',
        '../docs/themes/overlays.md',
        '../docs/themes/panel.md',
        '../docs/themes/programs.md',
        '../docs/themes/window-frame.md',
    ),
    install: true,
    install_dir: mavernix_datadir,
)
