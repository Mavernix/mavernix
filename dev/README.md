# Tools for working on Mavernix

| File | What it is for |
|---|---|
| `run-in-container` | Builds the desktop in a throwaway Podman container and opens it in a VNC window, without installing anything. `dev/run-in-container stop` ends it. |
| `Containerfile`, `container-session.sh` | The container's image, and what runs inside it: build, install, start the session. |

What the container writes (logs, the session's state, the VNC socket) goes into `dev/out/`, which git ignores.
