# Fedora packaging

The workflow builds Fedora 44 x86_64 packages, runs upstream tests and checks
installation of the four runtime RPMs together in a fresh container.
Graphical login and theme import were also tested manually in a Fedora KDE VM.

## CI

Push changes to main or fedora-rpm, or start Fedora RPM manually.
The workflow must exist on the default branch for the manual start button.
CI produces runtime and matching source artifacts, retained for seven days.

Mavernix is built from the checked-out commit. Its RPM version is read from
meson.build; the tracked spec is a template. Development builds of the same
version are for testing and are not distinguishable updates to DNF.

## Release

Merge the packaging changes before tagging a release. Set the project version
in meson.build, commit it, then push the matching stable tag (vX.Y.Z).
The tag must contain this workflow and the packaging files. Tag and Meson
versions must agree; mismatches fail the build.

After all builds and the installation check succeed, the release job attaches
only the sources tar.gz bundle, containing all four source RPMs, build files,
README and checksums. Runtime RPMs stay in the matching GitHub Actions artifacts.

It creates a GitHub Release if one does not exist. On reruns, the matching
source archive is replaced. The job does not publish for ordinary branch
pushes or manual runs. GitHub also displays its automatic source-code downloads.
Do not rerun old release jobs expecting this new layout: old tags contain the
workflow that existed when they were created.

No personal token is needed: only the release job gets contents:write.
It publishes in the repository where the tag was pushed; a fork does not
observe tags created in the upstream repository.

Keep source downloads available for as long as the binary packages are offered.
Do not replace a source bundle with sources from another build.

## Dependencies

Wayfire and wf-config are pinned to 0.11.0, Quickshell to 0.3.2.
Wayfire's source archive includes pinned wf-json, wf-touch and wf-utils submodules.
Their license notices are included in the RPMs.
Quickshell's optional crash reporter is disabled because cpptrace is unavailable
in Fedora 44. The remaining features and tests stay enabled.

Rebuild and retest when changing Wayfire or Qt: the compositor plugins and
Quickshell use APIs that can change between releases. Supporting a new Fedora
version requires updating and testing the build target explicitly.
