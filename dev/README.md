# Tools for working on Mavernix

| File | What it is for |
|---|---|
| `run-in-container` | Builds the desktop in a throwaway Podman container and opens it in a VNC window, without installing anything. `dev/run-in-container stop` ends it. |
| `Containerfile`, `container-session.sh` | The container's image, and what runs inside it: build, install, start the session. |
| `Containerfile.fedora` | The image for `MAVERNIX_FEDORA=44 dev/run-in-container` (or 45): it builds the Fedora packages from `packaging/fedora` and runs the desktop from them. |

What the container writes (logs, the session's state, the VNC socket) goes into `dev/out/`, which git ignores.
