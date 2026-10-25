# Contributing

Thank you for helping. A few things keep the project in shape.

## Before you start

For anything larger than a fix, open an issue first and say what you have in mind; it saves work on both sides. [How it is put together](docs/architecture.md) is the map; [looks](docs/looks.md) explains what the Theme Installer makes, and [themes](docs/themes/README.md) the theme format.

## Never commit other companies' files

Mavernix ships no Apple software or artwork, and must stay that way. Do not add to the repository, in any form:

- an installer, disk image or anything unpacked from one;
- a picture, icon, sound, font or desktop picture taken from such files, or a theme made from them (the Sea Lion themes the Theme Installer writes into `~/.local/share/mavernix/` included);
- a screenshot or recording that shows them;
- data decoded from them, pasted into a source file or a test.

Numbers and facts about a file format (an offset, a picture's number, a checksum) are fine; the files themselves are not. Tests use small files made for the test. `dev/out/`, where the container writes, is ignored by git for this reason.

## Building and checking

```sh
meson setup build
meson compile -C build
meson test -C build
dev/run-in-container        # see the desktop, without installing it (Podman and TigerVNC's vncviewer)
```

While the container runs:

- **QML** (`src/shell/`): `dev/run-in-container shell` copies your changes in and restarts the shell, in a few seconds.
- **Anything compiled** (`src/shell/native/`, the compositor plugins, the settings window, the Theme Installer's window) or **the session's scripts**: `dev/run-in-container stop`, then start it again; it builds from your working tree.
- **Logs**: `dev/out/build.log` (the build), `dev/out/state/session.log` (the compositor) and `dev/out/state/login.log` (the shell and what started with it).

Before a pull request:

```sh
ruff check .                                              # Python (ruff.toml)
clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.hpp')   # C++ layout (.clang-format)
```

A change to a theme or to the theme format: `mavernix-check-theme FOLDER` holds a theme to the format (the [themes](docs/themes/README.md#checking-a-theme) page). CI runs the build, the tests and both checks on every pull request.

## The rules of the code

- **Looks are not in the code.** A size, colour, picture, timing or word that a theme could want different belongs in the theme or the config, not in a source file. If a theme cannot say something, extend the [theme format](docs/themes/README.md) and document the new key; never ask which theme is loaded.
- **Generic names.** Parts are named for what they do (`AppGrid`, `WindowOverview`, `Search`), not after another system's products.
- **Readable over clever.** Plain control flow, names that say what things are, units in names where they are not obvious (`duration_ms`, `inset_px`). Each file starts with a short comment saying what it is for.
- **Comment the why.** Not what the code plainly does, but every non-obvious decision, every workaround (with the bug it works around), and every number that is not self-explanatory.
- **No dead code.** Remove what is unused rather than commenting it out.
- **Match the surroundings**: the style of the file and the conventions of its language.
- **Fail clearly.** An error message says what went wrong and what to check.
- **Tests for logic that can break quietly**, in `tests/`: parsers, geometry, rules. Not for pixels.
- **Layout is the tools' job.** `clang-format` and EditorConfig settle spaces and braces; the plugins in `src/compositor/decoration/` and `minimize/` keep Wayfire's layout (`.clang-format-ignore`).

## Reporting bugs

Use the bug report form on GitHub: it asks for the distribution, the versions of Wayfire and Quickshell, and the end of the logs (`$XDG_RUNTIME_DIR/mavernix/session.log` and `login.log`; for the Theme Installer `~/.cache/mavernix/theme-installer.log`). A security problem goes privately to the maintainers instead, as [SECURITY.md](SECURITY.md) says.

## Commits

One change per commit, with a subject in the imperative that starts with `add:`, `fix:` or `chore:`, for example `fix: the dock keeps its size after a theme switch`.

## Licence

By contributing you agree that your work is published under the GNU General Public License, version 3 or later.
