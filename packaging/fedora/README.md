# Experimental Fedora 44 RPM builds

These are community builds for x86_64, not official Fedora packages.
No Apple installers, artwork, sounds, fonts or imported themes are included.

## Downloads and corresponding sources

Each successful installation-test run produces two companion artifacts:

- `mavernix-fedora-44-runtime`: the four runtime RPMs and checksums.
- `mavernix-fedora-44-sources`: the four matching source RPMs, packaging
  specs, workflow and checksums.

Download both from the same GitHub Actions run. CI artifacts currently expire
after seven days. They are test outputs, not a permanent release channel.

Before publishing binary RPMs in a GitHub Release, attach the matching source
RPMs and build instructions to that same release. Keep those sources available
while distributing the binaries. Do not publish the runtime artifact alone
as a permanent download while leaving sources only in expiring CI artifacts.

Source RPMs contain the upstream source archives and packaging specs, including
our wf-config test fix. The Wayfire source archive also contains the pinned
wf-json, wf-touch and wf-utils submodules. The workflow records how it is made.

## Licenses

- Mavernix: GPL-3.0-or-later, with MIT-licensed compositor portions.
- Wayfire, wf-config, wf-utils, wf-json and wf-touch: MIT.
- Quickshell: see its bundled license texts and per-file third-party notices;
  the package license expression is recorded in quickshell.spec.

License and copyright notices must be retained. Before a release, review all
bundled third-party components and ensure their required notices are included.
These packaging checks are not a comprehensive legal audit.

## Install after validation

In the extracted runtime directory:

```sh
sha256sum -c SHA256SUMS
sudo dnf install ./*.rpm
```

A successful container installation does not confirm a working graphical login.
A Fedora desktop test is still required before calling these packages supported.
