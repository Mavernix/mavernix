# Security

## Reporting a problem

Report it privately: on GitHub, the repository's **Security** tab, **Report a vulnerability**. Please do not open a public issue for it. This is a small project kept in spare time; a report is answered as soon as possible, and fixed before it is made public.

## What counts

Above all, anything in the **Theme Installer** (`src/apps/theme-installer/`): it reads disk images and archives that come from outside, and runs 7-Zip, cpio, bzip2 and others on them, inside a bubblewrap sandbox when one can be made. A file that makes it write outside its own folders, run something from the image, or escape the sandbox is a security problem.

Also: the session's scripts (`src/session/`), which run at every login; the lock screen; anything that lets one user's settings affect another user.

## Supported versions

The latest release only.
