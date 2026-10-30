# The tests: the logic that needs no screen, one file per subject.
#
#   meson test -C build

test_sources = '..' / 'src'

tests = {
    'snap-keys': {
        'sources': [test_sources / 'compositor/snap-keys/snap-steps.cpp'],
        'include': test_sources / 'compositor/snap-keys',
        'dependencies': [],
    },
    'shared-edges': {
        'sources': [test_sources / 'compositor/shared-edges/seam-math.cpp'],
        'include': test_sources / 'compositor/shared-edges',
        'dependencies': [],
    },
    'theme-file': {
        'sources': [test_sources / 'compositor/decoration/theme-file.cpp'],
        'include': test_sources / 'compositor/decoration',
        'dependencies': [tomlplusplus],
    },
    'keyboard': {
        'sources': [
            test_sources / 'apps/settings/shortcut-keys.cpp',
            test_sources / 'apps/settings/keyboard-layouts.cpp',
        ],
        'include': test_sources / 'apps/settings',
        'dependencies': [qt_widgets],
    },
    'panel-modes': {
        'sources': [test_sources / 'apps/settings/panel-modes.cpp'],
        'include': test_sources / 'apps/settings',
        'dependencies': [],
    },
    'icon-theme-names': {
        'sources': [test_sources / 'apps/settings/icon-theme-names.cpp'],
        'include': test_sources / 'apps/settings',
        'dependencies': [qt_widgets],
    },
    'screen-layout': {
        'sources': [test_sources / 'apps/settings/screen-layout.cpp'],
        'include': test_sources / 'apps/settings',
        'dependencies': [],
    },
}

foreach name, test_parts : tests
    test_file = 'test-' + name + '.cpp'
    executable_file = executable(
        'test-' + name,
        [test_file, test_parts['sources'], qt.compile_moc(sources: test_file, dependencies: qt_test)],
        include_directories: include_directories(test_parts['include']),
        dependencies: [qt_test, test_parts['dependencies']],
        # The shipped themes, for the theme file's test.
        cpp_args: ['-DTHEMES_DIR="' + (meson.project_source_root() / 'data' / 'themes') + '"'],
    )
    test(name, executable_file)
endforeach

python = find_program('python3')
test(
    'session-config',
    python,
    args: [files('test_session_config.py')],
    env: {
        'SESSION_SOURCE_DIR': meson.project_source_root() / 'src' / 'session',
        'CONFIG_SOURCE_DIR': meson.project_source_root() / 'data' / 'config',
        'THEMES_SOURCE_DIR': meson.project_source_root() / 'data' / 'themes',
        'THEME_DOCS_DIR': meson.project_source_root() / 'docs' / 'themes',
        'LOOKS_SOURCE_DIR': meson.project_source_root() / 'src' / 'apps' / 'theme-installer' / 'backend' / 'looks',
    },
)

test(
    'app-store',
    python,
    args: [files('test_app_store.py')],
    env: {'APP_STORE_BACKEND_DIR': meson.project_source_root() / 'src' / 'apps' / 'app-store' / 'backend'},
)
