<p align="center">
  <img src="data/icons/mavernix.svg" alt="Mavernix" width="160">
</p>

<h1 align="center">Mavernix</h1>

<p align="center">
  <b>Turns your desktop more… lickable.</b><br>
  <a href="docs/building.md">Install</a> · <a href="docs/usage.md">Use</a> · <a href="docs/themes/README.md">Themes</a> · <a href="CONTRIBUTING.md">Contribute</a>
</p>

Mavernix is a desktop for Linux with a menu bar, a dock, and windows with proper shadows.

Out of the box it is calm and plain. If you still have an old installer of a classic system lying around, the Theme Installer turns it into a full look on your own computer:

| Your installer | Turns into | Status |
|---|---|---|
| OS X 10.9.5 Mavericks | Sea Lion, light and dark | supported |
| Mac OS X 10.3 Panther | | soon |
| Mac OS 9 | | soon |

Mavernix ships none of Apple's files; each look is built from your copy of the installer.

## Install

Mavernix is built and tested on Arch Linux. The dependencies are listed in [docs/building.md](docs/building.md).

```sh
meson setup build && meson compile -C build && sudo meson install -C build
```

Then pick Mavernix at your login screen.

You can also try it without installing: `dev/run-in-container` runs it in a Podman container.

## Contributing

Bug reports, themes and fixes to the docs are welcome, no C++ needed. See [CONTRIBUTING.md](CONTRIBUTING.md).

## Docs

[Usage](docs/usage.md) · [Configuration](docs/configuration.md) · [Themes](docs/themes/README.md) · [Looks](docs/looks.md) · [Panel items](docs/panel-items.md) · [Extensions](docs/extensions.md) · [Sound packs](docs/sound-packs.md) · [Architecture](docs/architecture.md)

## License

GPL-3.0-or-later, see [LICENSE](LICENSE). Two compositor plugins are based on Wayfire's and keep its MIT license too (`src/compositor/*/LICENSE-wayfire`).

Mavernix is not affiliated with or endorsed by Apple Inc. Mac and OS X are trademarks of Apple Inc.
