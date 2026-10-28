# Contributing

Thanks for wanting to help! :) Bug reports, themes, docs fixes and code are all welcome, and plenty of it needs no C++ at all. Here's what keeps things running smoothly.

## Before you start

If it's more than a small fix, please open an issue first and tell us what you have in mind. It saves work on both sides.

To find your way around, [how the desktop is put together](docs/architecture.md) is the map. [Looks](docs/looks.md) explains what the Theme Installer builds, and [themes](docs/themes/README.md) covers the theme format.

## Please don't commit Apple's files

Mavernix doesn't ship any Apple software or artwork, and it needs to stay that way. So please don't add any of these to the repository:

- an installer, a disk image, or anything unpacked from one
- a picture, icon, sound, font or desktop picture taken from those files, or a theme made from them. That includes the Sea Lion themes the Theme Installer writes into `~/.local/share/mavernix/`.
- a screenshot or recording that shows them
- data decoded from them and pasted into a source file or a test

Numbers and facts about a file format are fine: an offset, a picture's number, a checksum. The files themselves aren't. Tests use small files made just for the test. That's also why git ignores `dev/out/`, where the container writes its output.

## Building and checking

```sh
meson setup build
meson compile -C build
meson test -C build
dev/run-in-container        # see the desktop without installing it (needs Podman and TigerVNC's vncviewer)
```

While the container is running:

- **QML** (`src/shell/`): `dev/run-in-container shell` copies your changes in and restarts the shell. It takes a few seconds.
- **Anything compiled** (`src/shell/native/`, the compositor plugins, the Settings window, the Theme Installer's window) or **the session's scripts**: run `dev/run-in-container stop`, then start it again. It builds from your working tree.
- **Logs**: `dev/out/build.log` for the build, `dev/out/state/session.log` for the compositor, and `dev/out/state/login.log` for the shell and everything started with it.

Before you open a pull request, run:

```sh
ruff check .                                              # Python (ruff.toml)
clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.hpp')   # C++ layout (.clang-format)
```

Changed a theme or the theme format? `mavernix-check-theme FOLDER` checks a theme against the format (see the [themes](docs/themes/README.md#checking-a-theme) page). CI runs the build, the tests and both checks on every pull request.

## How we write code

- **Looks don't go in the code.** If a theme could want a size, colour, picture, timing or word to be different, it belongs in the theme or the config, not in a source file. If a theme can't express something yet, extend the [theme format](docs/themes/README.md) and document the new key. Never check which theme is loaded.
- **Generic names.** Name parts after what they do (`AppGrid`, `WindowOverview`, `Search`), not after another system's products.
- **Readable beats clever.** Plain control flow, names that say what things are, and units in names when they aren't obvious (`duration_ms`, `inset_px`). Start each file with a short comment saying what it's for.
- **Comment the why.** Skip what the code plainly does. Do explain every decision that isn't obvious, every workaround (and the bug it works around), and every number that doesn't explain itself.
- **No dead code.** If something's unused, remove it instead of commenting it out.
- **Match what's around you**: the style of the file and the habits of its language.
- **Fail clearly.** An error message says what went wrong and what to check.
- **Test the logic that can break quietly**, in `tests/`: parsers, geometry, rules. Not pixels.
- **Let the tools handle layout.** `clang-format` and EditorConfig settle spaces and braces. The plugins in `src/compositor/decoration/` and `minimize/` keep Wayfire's own layout (`.clang-format-ignore`).

## Reporting bugs

Please use the bug report form on GitHub. It asks for your distribution, your Wayfire and Quickshell versions, and the end of the logs:

- `$XDG_RUNTIME_DIR/mavernix/session.log` and `login.log`
- for the Theme Installer, `~/.cache/mavernix/theme-installer.log`

Found a security problem? Please report it privately to the maintainers instead, as [SECURITY.md](SECURITY.md) explains.

## Commits

One change per commit. Start the subject with `add:`, `fix:` or `chore:`, for example `fix: the dock keeps its size after a theme switch`.

## Licence

By contributing, you agree that your work is published under the GNU General Public License, version 3 or later.
