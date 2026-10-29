# Fedora packages

For Fedora 44 and 45, x86_64. Fedora's Wayfire and wf-config are 0.10, older than the 0.11 Mavernix needs, and its Quickshell is a 0.2 snapshot, so these specs build all three as well:

| Spec | What |
|---|---|
| `wf-config.spec` | wf-config 0.11.0, with a fix for its lock test (`wf-config-lock-test.patch`) |
| `wayfire.spec` | Wayfire 0.11.0, with the submodules its tag pins |
| `quickshell.spec` | Quickshell 0.3.2, without the crash reporter: it needs cpptrace, which Fedora doesn't package |
| `mavernix.spec` | Mavernix itself. Its version goes up with `meson.build`'s and the `PKGBUILD`'s; `build-rpms` stops if they disagree. |

The three dependencies have a release below 1, so Fedora's own builds of the same versions replace them.

Installing them replaces Fedora's Wayfire and Quickshell. Fedora's wf-shell, wcm and wayfire-plugins-extra are built for its Wayfire 0.10, so dnf offers to remove them; and other Quickshell setups on the machine move to Quickshell 0.3.

## Building

`build-rpms` builds all four inside Fedora, Mavernix from the source tree as it is:

```sh
podman run --rm -v "$PWD:/src:ro" -v "$PWD/rpms:/out" \
    registry.fedoraproject.org/fedora:44 /src/packaging/fedora/build-rpms /out
```

CI does the same on every push (`.github/workflows/fedora.yml`); each run keeps the packages for 14 days, for testing. To try the desktop from them in a VNC window, without a Fedora machine: `MAVERNIX_FEDORA=44 dev/run-in-container`.

## Publishing

For people to install it, these specs belong in a [COPR](https://copr.fedorainfracloud.org) repository: COPR builds them for each Fedora release (wf-config first, then Wayfire, then Quickshell and Mavernix) and serves a dnf repository, so updates come with `dnf upgrade`. Mavernix is built again from each release's tag; the other three when their spec changes, and for each new Fedora release.
