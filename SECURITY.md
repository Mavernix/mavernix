# Security

## Reporting a problem

Please report it privately. On GitHub, go to the repository's **Security** tab and click **Report a vulnerability**. Please don't open a public issue for it.

Mavernix is a small project, kept up in people's spare time. We'll answer your report as soon as we can, and fix the problem before it's made public.

## What counts

Most of all, anything in the **Theme Installer** (`src/apps/theme-installer/`). It reads disk images and archives that come from outside, and runs 7-Zip, cpio, bzip2 and other tools on them, inside a bubblewrap sandbox when it can set one up. A file that makes it write outside its own folders, run something from the image, or escape the sandbox is a security problem.

These count too:

- the session's scripts (`src/session/`), which run at every login
- the lock screen
- anything that lets one user's settings affect another user

## Supported versions

Only the latest release.
