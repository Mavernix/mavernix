"""Fetching from the web for the App Store, with nothing but Python's urllib.

GitHub's API answers a computer that hasn't signed in 60 times an hour. So
every answer is kept in the cache with its ETag, and asked for again with that
tag: when nothing changed, GitHub answers "304 Not Modified", which doesn't
count against the limit, and the kept answer is used.
"""

import hashlib
import http.client
import json
import os
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

USER_AGENT = "Mavernix-App-Store"
TIMEOUT_SECONDS = 30
CHUNK_BYTES = 64 * 1024


class StoreProblem(Exception):
    """Something went wrong that the user should read about, in words they understand."""


def api_base():
    # MAVERNIX_APP_STORE_API points the store at another copy of GitHub's API,
    # such as the tests' local server.
    return os.environ.get("MAVERNIX_APP_STORE_API", "https://api.github.com").rstrip("/")


def cache_folder():
    cache = os.environ.get("XDG_CACHE_HOME") or Path.home() / ".cache"
    return Path(cache) / "mavernix" / "app-store"


def _request(url, headers=None):
    return urllib.request.Request(url, headers={"User-Agent": USER_AGENT, **(headers or {})})


def _open(request):
    """Opens `request`, turning the ways it can fail into a StoreProblem."""
    try:
        return urllib.request.urlopen(request, timeout=TIMEOUT_SECONDS)
    except urllib.error.HTTPError as error:
        if error.code == 304:
            raise
        if error.code in (403, 429) and error.headers.get("X-RateLimit-Remaining") == "0":
            reset = int(error.headers.get("X-RateLimit-Reset", "0"))
            raise StoreProblem(
                "GitHub has answered as many questions from this computer as it allows for now. "
                f"Try again after {time.strftime('%H:%M', time.localtime(reset))}."
            ) from error
        raise StoreProblem(f"GitHub answered {request.full_url} with {error.code} {error.reason}.") from error
    except (urllib.error.URLError, TimeoutError, OSError) as error:
        reason = getattr(error, "reason", error)
        raise StoreProblem(f"Couldn't reach {urllib.parse.urlsplit(request.full_url).netloc}: {reason}.") from error


def get_json(path):
    """The API's answer for `path` (such as "orgs/Mavernix/repos"), from the cache if it hasn't changed."""
    url = f"{api_base()}/{path}"
    kept_file = cache_folder() / "answers" / (hashlib.sha256(url.encode()).hexdigest() + ".json")
    kept = None
    if kept_file.is_file():
        kept = json.loads(kept_file.read_text())

    headers = {"Accept": "application/vnd.github+json"}
    if kept is not None and kept.get("etag"):
        headers["If-None-Match"] = kept["etag"]
    try:
        with _open(_request(url, headers)) as response:
            answer = json.load(response)
            etag = response.headers.get("ETag")
    except urllib.error.HTTPError as error:
        # Only a 304 gets here: nothing changed since the kept answer.
        if error.code == 304 and kept is not None:
            return kept["answer"]
        raise StoreProblem(f"GitHub answered {url} with {error.code} {error.reason}.") from error

    kept_file.parent.mkdir(parents=True, exist_ok=True)
    kept_file.write_text(json.dumps({"etag": etag, "answer": answer}))
    return answer


def download(url, destination, sha256=None, on_progress=None):
    """Downloads `url` to the file `destination`.

    With `sha256`, the file must have that checksum, or it isn't kept.
    `on_progress(done_bytes, total_bytes)` is called as it arrives; total_bytes
    is 0 when the server doesn't say.
    """
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    partial = destination.with_name(destination.name + ".part")
    checksum = hashlib.sha256()

    with _open(_request(url)) as response, open(partial, "wb") as file:
        total = int(response.headers.get("Content-Length") or 0)
        done = 0
        try:
            while chunk := response.read(CHUNK_BYTES):
                file.write(chunk)
                checksum.update(chunk)
                done += len(chunk)
                if on_progress is not None:
                    on_progress(done, total)
        except (http.client.HTTPException, ConnectionError, TimeoutError) as error:
            # The connection broke off partway; what arrived isn't kept.
            raise StoreProblem(f"{destination.name} stopped arriving partway ({error}). Try again.") from error

    if sha256 is not None and checksum.hexdigest() != sha256:
        partial.unlink()
        raise StoreProblem(f"{destination.name} didn't arrive intact (its checksum is wrong). Try again.")
    partial.replace(destination)
    return destination
