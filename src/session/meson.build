# The session scripts: start the desktop and write the compositor's and the
# shell's settings from a desktop config and its theme.

session_dir = mavernix_libdir / 'session'

paths = configuration_data()
paths.set('LIBRARY_DIR', get_option('prefix') / mavernix_libdir)
paths.set('DATA_DIR', get_option('prefix') / mavernix_datadir)
configure_file(input: 'paths.sh.in', output: 'paths.sh', configuration: paths,
    install_dir: session_dir)
configure_file(input: 'paths.py.in', output: 'paths.py', configuration: paths,
    install_dir: session_dir)

install_data(
    'start-desktop',
    'configure-desktop',
    'run-shell',
    'keep-running',
    'after-session',
    'compositor_call.py',
    install_dir: session_dir,
    install_mode: 'rwxr-xr-x',
)
install_data(
    'at_login.py',
    'check_theme.py',
    'make_app_theme.py',
    'make_default_programs.py',
    'make_shell_config.py',
    'make_wayfire_config.py',
    'pick_desktop.py',
    'refresh_desktops.py',
    'themes.py',
    'toml_text.py',
    'user_choices.py',
    'write_interface_settings.py',
    'kded6rc',
    install_dir: session_dir,
)
install_data('bin/mavernix-switch', 'bin/mavernix-reconfigure', 'bin/mavernix-file-action',
    'bin/mavernix-check-theme',
    install_dir: session_dir / 'bin', install_mode: 'rwxr-xr-x')
install_data('notice/shell.qml', install_dir: session_dir / 'notice')
install_data('portals/wayfire-portals.conf', install_dir: session_dir / 'portals')

# The login screen's entry.
configure_file(
    input: 'mavernix.desktop.in',
    output: 'mavernix.desktop',
    configuration: paths,
    install_dir: get_option('datadir') / 'wayland-sessions',
)
