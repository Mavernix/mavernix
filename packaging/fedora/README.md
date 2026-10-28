# Mavernix for Fedora

For Fedora 44, x86_64. Update Fedora and reboot before installing.

Download the runtime artifact from the matching GitHub Actions run.
Extract it, open a terminal in that folder and run:

```sh
sha256sum -c SHA256SUMS
sudo dnf install ./*.rpm
```

Log out and choose **Mavernix** on the login screen.

The download includes Mavernix, Wayfire, wf-config and Quickshell.
To import a theme, provide your own supported installer DMG in the Theme Installer.

Matching source RPMs and build instructions are in the separate sources download.
These are community packages. Quickshell's built-in crash reporter is disabled.
