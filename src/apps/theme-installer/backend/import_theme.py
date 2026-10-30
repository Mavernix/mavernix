"""Makes a look's themes from an installer the user owns.

    python import_theme.py check                what is missing to import, and
                                                the looks with their state, as JSON
    python import_theme.py identify SOURCE      which look SOURCE makes, as JSON
    python import_theme.py import SOURCE        the whole import
    python import_theme.py fingerprint SOURCE   the SHA-256 of every file the
                                                look needs, as needed-files.txt
                                                lists them (to add another
                                                version of the installer)
    python import_theme.py news                 the imported looks a new import
                                                would bring something to, and
                                                what, as JSON (updates.py);
                                                none the user put off
    python import_theme.py later LOOK...        puts off offering these looks'
                                                new import until an update
                                                brings something more

Each look is a folder in looks/ (look.toml says what is in it). SOURCE is
whatever its look.toml recognises: for Sea Lion, "Install OS X Mavericks.app",
its InstallESD.dmg, or a disk image made from either. The import writes the
look's themes into ~/.local/share/mavernix/themes/ and its desktops into
~/.local/share/mavernix/config/, where the session finds them.

What it is doing is reported on standard output, one JSON object per line,
for the import window to show (see Report below), and written to
~/.cache/mavernix/theme-installer.log. The work that reads the installer runs
in a sandbox when bubblewrap is installed: no network, and the whole file
system read-only except the import's own two folders.
"""

import hashlib
import json
import os
import shutil
import signal
import subprocess
import sys
import threading
import time
import tomllib
from pathlib import Path

import installer_image
import updates
from installer_image import InstallerProblem

BACKEND_DIR = Path(__file__).resolve().parent

LOOKS_DIR = BACKEND_DIR / "looks"

# Share of unpacking spent copying the parts out of the image; the rest is
# taking the files from them, which decompresses each whole payload.
COPY_SHARE = 0.3

# The steps of an import, as the window lists them, with a rough share of the
# whole time each takes.
STAGES = [
    ("open", "Opening the installer", 1),
    ("unpack", "Checking and unpacking the installer", 70),
    ("decode", "Reading the artwork", 8),
    ("theme", "Building the theme", 6),
    ("icons", "Converting the icons", 8),
    ("cursors", "Drawing the mouse pointers", 2),
    ("sounds", "Converting the sounds", 2),
    ("dark", "Building the dark theme", 3),
    ("install", "Installing", 2),
]

# What the import needs on this machine: (program, Arch package, what for).
REQUIRED_PROGRAMS = [
    ("7z", "7zip", "reads the installer's disk images and packages"),
    ("bunzip2", "bzip2", "decompresses the packages"),
    ("cpio", "cpio", "unpacks the packages"),
    ("icns2png", "libicns", "converts the icons"),
    ("ffmpeg", "ffmpeg", "converts the sounds"),
    ("pdftocairo", "poppler", "converts drawings into pictures"),
]
SANDBOX_PROGRAM = ("bwrap", "bubblewrap", "keeps the installer's files away from the rest of the system")

# Free space needed: the installer's parts, copied out together (4.4 GB), with
# room for the files taken from them; and the finished themes.
WORK_SPACE_BYTES = 5_000_000_000
TAKEN_FILES_BYTES = 300_000_000
THEME_SPACE_BYTES = 500_000_000

# How many images' lists of files are kept (listings/), so that the import
# does not read an image again only to list it.
KEPT_LISTINGS = 4

# The least time between two progress reports.
REPORT_INTERVAL_S = 0.2


class Report:
    """Writes what the import is doing as JSON lines:

        {"event": "plan", "stages": [{"id", "label", "weight"}...]}
        {"event": "stage", "id": STAGE}             a stage begins
        {"event": "progress", "fraction": 0.42}     how far the stage is
        {"event": "detail", "text": "..."}          what it is doing just now
        {"event": "note", "text": "..."}            to be shown at the end
        {"event": "done"} or {"event": "error", "message": "..."}
    """

    def __init__(self, log_path=None, quiet=False):
        self.quiet = quiet
        self.last_report = 0.0
        self.part_range = (0.0, 1.0)
        self.log = open(log_path, "a") if log_path else None

    def send(self, **event):
        if self.quiet:
            return
        print(json.dumps(event), flush=True)
        if event["event"] in ("stage", "detail", "note", "error", "done"):
            self.write_log(json.dumps(event))

    def write_log(self, text):
        """Adds lines to the log only, not to what the window is told."""
        if not self.log:
            return
        stamp = time.strftime("%Y-%m-%d %H:%M:%S")
        for line in text.rstrip("\n").split("\n"):
            self.log.write(f"{stamp} {line}\n")
        self.log.flush()

    def plan(self):
        self.send(event="plan", stages=[{"id": i, "label": label, "weight": w} for i, label, w in STAGES])

    def stage(self, stage_id):
        self.part_range = (0.0, 1.0)
        self.send(event="stage", id=stage_id)

    def detail(self, text):
        self.send(event="detail", text=text)

    def within(self, start, end):
        """Makes later fractions count for the share start..end of the stage."""
        self.part_range = (start, end)

    def fraction(self, fraction):
        now = time.monotonic()
        if now - self.last_report < REPORT_INTERVAL_S and fraction < 1:
            return
        self.last_report = now
        start, end = self.part_range
        self.send(event="progress", fraction=round(start + (end - start) * min(fraction, 1.0), 4))


def missing_programs():
    """The programs the import needs that are missing, and the sandbox's state."""
    missing = [{"program": program, "package": package, "purpose": purpose}
               for program, package, purpose in REQUIRED_PROGRAMS if shutil.which(program) is None]
    try:
        import PIL  # noqa: F401  (only its presence is checked)
    except ImportError:
        missing.append({"program": "Pillow", "package": "python-pillow", "purpose": "cuts and converts pictures"})
    return {"missing": missing, "sandbox": sandbox_works(),
            "sandbox_installed": shutil.which(SANDBOX_PROGRAM[0]) is not None,
            "sandbox_package": SANDBOX_PROGRAM[1]}


def read_looks():
    """{look id: look.toml's table, with `folder` added}, for every look."""
    looks = {}
    for look_file in sorted(LOOKS_DIR.glob("*/look.toml")):
        with open(look_file, "rb") as file:
            look = tomllib.load(file)
        look["folder"] = look_file.parent
        looks[look_file.parent.name] = look
    return looks


def look_states():
    """What the window lists: each look, whether its themes are installed,
    and what a new import would bring them (updates.py)."""
    states = []
    for look_id, look in read_looks().items():
        installed = all((themes_dir() / theme / "theme.toml").is_file() for theme in look["themes"])
        states.append({"id": look_id, "name": look["name"], "description": look["description"],
                       "source": look["source"], "drop_hint": look["drop_hint"],
                       "licence_note": look.get("licence_note", ""), "desktop": look["desktops"][0],
                       "installed": installed, "import_brings": updates.import_brings(look, themes_dir()),
                       "offer_import": updates.offers_import(look_id, look, themes_dir())})
    return states


def news():
    """The imported looks a new import would bring something to, unless the
    user put that off: [{id, name, import_brings}]."""
    looks = []
    for look_id, look in read_looks().items():
        if updates.offers_import(look_id, look, themes_dir()):
            looks.append({"id": look_id, "name": look["name"],
                          "import_brings": updates.import_brings(look, themes_dir())})
    return looks


def put_off(look_ids):
    """Remembers "Later" for these looks, up to their newest update with
    something to import."""
    looks = read_looks()
    unknown = [look_id for look_id in look_ids if look_id not in looks]
    if unknown:
        sys.exit(f"import_theme: no look called {', '.join(unknown)}; the looks are {', '.join(looks)}")
    updates.put_off({look_id: updates.newest_import_revision(updates.read_updates(looks[look_id]["folder"]))
                     for look_id in look_ids})


def holds(paths, suffix):
    """Whether an archive's list of paths has a file ending in `suffix`,
    whatever volume or folder is around it."""
    return any(path == suffix or path.endswith("/" + suffix) for path in paths)


def look_image(source, look):
    """The image of `look`'s installer in SOURCE: SOURCE, or the image inside
    a program folder."""
    return source / look["inside_program"] if source.is_dir() else source


def identify(source, list_image=installer_image.list_archive):
    """The id of the look whose installer SOURCE is. Reads only the image's
    list of files (with `list_image`); 7-Zip still reads the whole image for
    that, which takes minutes from a slow disk."""
    source = Path(source)
    listed = {}
    for look_id, look in read_looks().items():
        image = look_image(source, look)
        if not image.is_file():
            continue
        if image not in listed:
            try:
                listed[image] = [member.path for member in list_image(image)]
            except InstallerProblem:
                listed[image] = []
        paths = listed[image]
        if all(holds(paths, suffix) for suffix in look["recognise"]) or holds(paths, look["wrapped_in"]):
            return look_id
    raise InstallerProblem(f"{source.name} is not an installer the Theme Installer knows. "
                           "The looks it can make, and what each is made from, are on its first page.")


def user_folder(variable, fallback):
    return Path(os.environ.get(variable) or Path.home() / fallback)


def themes_dir():
    return user_folder("XDG_DATA_HOME", ".local/share") / "mavernix" / "themes"


def cache_dir():
    return user_folder("XDG_CACHE_HOME", ".cache") / "mavernix"


def log_path():
    return cache_dir() / "theme-installer.log"


def listings_dir():
    return cache_dir() / "listings"


def listing_file(image):
    """Where the list of files of `image` is kept: named after the image's
    path, size and time of change, so a changed image is listed anew."""
    image = Path(image).resolve()
    status = image.stat()
    key = f"{image}\0{status.st_size}\0{status.st_mtime_ns}"
    return listings_dir() / (hashlib.sha256(key.encode(errors="surrogateescape")).hexdigest() + ".json")


class NotListedYet(Exception):
    """An image whose list of files is not kept."""


def known_listing(image):
    """The kept list of files of `image`, or None."""
    try:
        with open(listing_file(image)) as file:
            return [installer_image.Member(path, size) for path, size in json.load(file)]
    except (OSError, ValueError):
        return None


def kept_listing(image):
    """known_listing, for identify: raises NotListedYet instead of None."""
    members = known_listing(image)
    if members is None:
        raise NotListedYet(image)
    return members


def keep_listing(image, members):
    """Keeps the list of files of `image`; only the newest few are kept."""
    folder = listings_dir()
    folder.mkdir(parents=True, exist_ok=True)
    target = listing_file(image)
    partial = target.with_suffix(".partial")
    partial.write_text(json.dumps([[member.path, member.size] for member in members]))
    partial.rename(target)
    for old in sorted(folder.glob("*.json"), key=lambda path: path.stat().st_mtime_ns, reverse=True)[KEPT_LISTINGS:]:
        old.unlink(missing_ok=True)


def run_tool(tool, *arguments):
    """Runs one of the decoders or generators beside this file."""
    result = subprocess.run([sys.executable, str(BACKEND_DIR / tool), *map(str, arguments)],
                            capture_output=True, text=True, cwd=BACKEND_DIR)
    if result.returncode != 0:
        # The whole message goes to the log (the outer process keeps this
        # one's error output); the window shows its end.
        print(f"{tool} failed:\n{result.stderr}", file=sys.stderr, flush=True)
        last_lines = "\n".join(result.stderr.strip().split("\n")[-5:])
        raise InstallerProblem(f"{tool} failed:\n{last_lines}")


def unpack(archive, parts, work, look_dir, report, check):
    """Takes the files the theme needs out of the installer's parts into
    WORK/root. With `check`, each part's files are checked as soon as they
    are out, so a wrong installer is refused early."""
    root = work / "root"
    root.mkdir(parents=True, exist_ok=True)
    files = installer_image.needed_files(look_dir)
    sizes = {name: parts[name].size for name in installer_image.PART_ORDER}
    total = sum(sizes.values())
    check_space(work, total + TAKEN_FILES_BYTES, "the installer's parts")

    report.detail(f"Copying the installer's parts out of {archive.name}")
    report.within(0, COPY_SHARE)
    copies = installer_image.copy_members(archive, [parts[name] for name in installer_image.PART_ORDER],
                                          work / "parts", report)
    # An image taken out of another is no longer needed.
    nested = work / "InstallESD.dmg"
    if nested.exists():
        nested.unlink()

    done = 0
    differing = []
    for name in installer_image.PART_ORDER:
        start = COPY_SHARE + (1 - COPY_SHARE) * done / total
        report.within(start, start + (1 - COPY_SHARE) * sizes[name] / total)
        part_file = copies[parts[name].path]
        report.detail(f"Taking the theme's files from {part_file.name}")
        if name == "BaseSystem":
            installer_image.take_files_from_base_system(part_file, list(files[name]), root, report)
        else:
            installer_image.take_files_from_package(part_file, list(files[name]), root, report)
        part_file.unlink()
        if check:
            differing += installer_image.differing_files(files[name], root)
        done += sizes[name]
    shutil.rmtree(work / "parts")
    if differing:
        verb = "differs" if len(differing) == 1 else "differ"
        report.send(event="note", text=f"{len(differing)} of the theme's pictures, icons or sounds {verb} "
                                       "from those of the 10.9.5 installer; the theme was made from them as they are.")
    return root


def fingerprint(source, work):
    """Prints needed-files.txt with the checksums of this installer's files."""
    look_dir = read_looks()[identify(source)]["folder"]
    archive, parts = installer_image.find_installer(source, work, Report(quiet=True))
    root = unpack(archive, parts, work, look_dir, Report(quiet=True), check=False)
    for part, files in installer_image.needed_files(look_dir).items():
        for path in files:
            print(f"{part}\t{installer_image.file_checksum(root / path)}\t{path}")


def build(source, work, staging, look_dir, report):
    """Everything that reads the installer, up to the finished themes in
    STAGING. This is the part that runs in the sandbox."""
    report.stage("open")
    archive, parts = installer_image.find_installer(source, work, report, known_listing)
    report.fraction(1)

    report.stage("unpack")
    root = unpack(archive, parts, work, look_dir, report, check=True)

    report.stage("decode")
    frameworks = root / "System/Library/PrivateFrameworks"
    coreui = frameworks / "CoreUI.framework/Versions/A"
    artwork = work / "artwork"
    itunes = root / "Applications/iTunes.app/Contents/Resources"
    decoders = [
        ("the controls' artwork", "artfile.py", coreui / "Resources/ArtFile.bin", artwork / "artfile-1x"),
        ("the controls' artwork at double density", "artfile.py", coreui / "Resources/ArtFile200.bin", artwork / "artfile-2x"),
        ("the shared artwork", "sartfile.py", coreui / "Resources/SArtFile.bin", artwork / "sartfile"),
        ("iTunes' pictures", "rsrcfile.py", itunes / "iTunes.rsrc", artwork / "itunes/iTunes"),
        ("iTunes' pictures at double density", "rsrcfile.py", itunes / "Images.rsrc", artwork / "itunes/Images"),
        ("more of iTunes' pictures at double density", "rsrcfile.py", itunes / "Images2.rsrc", artwork / "itunes/Images2"),
        ("the dark look's artwork", "carfile.py", frameworks / "ProKit.framework/Versions/A/Resources/ProThemeBits.car", artwork / "prokit"),
        ("the colour table", "coreui_colors.py", coreui / "CoreUI", work / "coreui-colors.json"),
    ]
    for number, (what, tool, source_file, output) in enumerate(decoders):
        report.detail(f"Reading {what}")
        run_tool(tool, source_file, output)
        report.fraction((number + 1) / len(decoders))

    light, dark = staging / "sea-lion", staging / "sea-lion-dark"
    for stage_id, detail, tool, arguments in [
        ("theme", "Window frames, the panel, the dock, menus and the programs", "make_theme.py", [work, light]),
        ("icons", "Icons for files, folders and programs", "make_icons.py", [work, light]),
        ("cursors", "The arrow, the text beam, the hands and the other pointers", "make_cursors.py", [work, light]),
        ("sounds", "The alert and event sounds", "make_sounds.py", [work, light]),
        ("dark", "The dark variant", "make_dark_theme.py", [work, light, dark]),
    ]:
        report.stage(stage_id)
        report.detail(detail)
        run_tool(tool, *arguments)
        report.fraction(1)


def check_themes(staging, look, report):
    """Holds the made themes to the theme format. A key it does not know is a
    mistake of the generator, reported but not fatal: the theme still works."""
    if shutil.which("mavernix-check-theme") is None:
        return
    for name in look["themes"]:
        result = subprocess.run(["mavernix-check-theme", str(staging / name)], capture_output=True, text=True)
        for line in result.stdout.splitlines():
            report.detail(f"Not in the theme format: {line}")


def install(staging, look, report):
    """Puts the finished themes in place and makes their typeface available.
    Runs outside the sandbox."""
    report.stage("install")
    check_themes(staging, look, report)
    target = themes_dir()
    for number, name in enumerate(look["themes"]):
        new = staging / name
        old = target / name
        updates.stamp_revision(new, updates.newest_revision(updates.read_updates(look["folder"])))
        replaced = target / f".{name}.replaced"
        shutil.rmtree(replaced, ignore_errors=True)
        if old.exists():
            old.rename(replaced)
        new.rename(old)
        shutil.rmtree(replaced, ignore_errors=True)
        report.fraction((number + 1) / (len(look["themes"]) + 1))

    # The look's desktops, the one "Use" switches to last: the session starts
    # the latest one added when the user has chosen none.
    configs = user_folder("XDG_DATA_HOME", ".local/share") / "mavernix" / "config"
    configs.mkdir(parents=True, exist_ok=True)
    for desktop in reversed(look["desktops"]):
        shutil.copyfile(look["folder"] / f"{desktop}.toml", configs / f"{desktop}.toml")
        # What was copied, so that a later version's refresh can tell it from
        # a copy changed by hand (src/session/refresh_desktops.py).
        copied = (configs / f"{desktop}.toml").read_bytes()
        (configs / f".{desktop}.toml.installed").write_text(hashlib.sha256(copied).hexdigest() + "\n")

    # A link in the user's font folder lets every program find a theme's
    # typeface. (A theme whose fonts/ links to another's shares that one's.)
    for name in look["themes"]:
        fonts = target / name / "fonts"
        if not fonts.is_dir() or fonts.is_symlink():
            continue
        report.detail("Making the typeface available")
        link = user_folder("XDG_DATA_HOME", ".local/share") / "fonts" / name
        link.parent.mkdir(parents=True, exist_ok=True)
        if link.is_symlink() or link.exists():
            link.unlink()
        link.symlink_to(fonts)
        if shutil.which("fc-cache"):
            subprocess.run(["fc-cache", str(link)], capture_output=True)
    report.fraction(1)


def sandbox_works():
    """Whether bubblewrap is installed and can make a sandbox here. (Inside
    some containers it cannot create the namespaces it needs.)"""
    if shutil.which("bwrap") is None:
        return False
    trial = subprocess.run(sandboxed(["true"], []), capture_output=True)
    return trial.returncode == 0


def sandboxed(command, writable, readable=()):
    """`command` inside bubblewrap: the file system read-only except the
    folders in `writable`, no network, and ended when this process ends.
    /tmp is empty inside, so what is read from there is named in `readable`
    (and this program's own folder is, for a copy run from there)."""
    sandbox = ["bwrap", "--ro-bind", "/", "/", "--dev", "/dev", "--proc", "/proc", "--tmpfs", "/tmp"]
    for path in [BACKEND_DIR, *readable]:
        if Path(path).exists():
            sandbox += ["--ro-bind", str(path), str(path)]
    for folder in writable:
        sandbox += ["--bind", str(folder), str(folder)]
    sandbox += ["--unshare-all", "--die-with-parent", "--new-session"]
    return sandbox + command


def check_space(folder, needed_bytes, purpose):
    folder.mkdir(parents=True, exist_ok=True)
    free = shutil.disk_usage(folder).free
    if free < needed_bytes:
        raise InstallerProblem(f"The import needs {needed_bytes / 1e9:.1f} GB free for {purpose} "
                               f"in {folder}, and there are {free / 1e9:.1f} GB.")


def inside_command(arguments, writable, readable=()):
    """This program with `arguments`, in the sandbox when there is one."""
    command = [sys.executable, str(Path(__file__).resolve())] + [str(argument) for argument in arguments]
    if sandbox_works():
        command = sandboxed(command, writable, readable)
    return command


def run_inside(arguments, writable, readable=(), capture=False):
    """Runs this program with `arguments`, in the sandbox when there is one;
    true if it succeeded. With `capture`, returns its output instead (or None)."""
    result = subprocess.run(inside_command(arguments, writable, readable), capture_output=capture, text=capture)
    if capture:
        if result.returncode != 0:
            print(result.stderr, file=sys.stderr, end="", flush=True)
            return None
        return result.stdout
    return result.returncode == 0


def relay_inside(arguments, writable, readable, report):
    """Runs this program with `arguments` like run_inside, passing what it
    reports on through `report`, so that it reaches the log too; true if it
    succeeded. Its error output goes to the log."""
    process = subprocess.Popen(inside_command(arguments, writable, readable),
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    errors = []

    def log_errors():
        for line in process.stderr:
            errors.append(line)
            report.write_log(f"import: {line}")

    logging = threading.Thread(target=log_errors, daemon=True)
    logging.start()
    told_why = False
    try:
        for line in process.stdout:
            try:
                event = json.loads(line)
            except ValueError:
                report.write_log(f"import: {line}")
                continue
            told_why = told_why or event.get("event") == "error"
            report.send(**event)
        logging.join()
        succeeded = process.wait() == 0
    finally:
        # Stopped from the window: without a sandbox nothing else ends it.
        if process.poll() is None:
            process.terminate()
            process.wait()
    if not succeeded and not told_why:
        last = errors[-1].strip() if errors else f"it ended with status {process.returncode}"
        report.send(event="error", message=f"The import stopped unexpectedly: {last}\n"
                                           f"The whole message is in {log_path()}.")
    return succeeded


def identify_outside(source):
    """{"look": id} or {"problem": message} for SOURCE. An image listed before
    is recognised from its kept list of files; any other is listed in the
    sandbox, and its list is kept for the import."""
    source = Path(source).resolve()
    try:
        return {"look": identify(source, kept_listing)}
    except NotListedYet:
        pass
    except InstallerProblem as problem:
        return {"problem": str(problem)}
    answer = run_inside(["identify-inside", source], [], readable=[source], capture=True)
    if answer is None:
        return {"problem": f"The Theme Installer could not read the file. What went wrong is in {log_path()}."}
    answer = json.loads(answer)
    for image, members in answer.pop("listings", {}).items():
        try:
            keep_listing(image, [installer_image.Member(path, size) for path, size in members])
        except OSError as problem:
            print(f"import_theme: could not keep the list of files of {image}: {problem}", file=sys.stderr)
    return answer


def identify_inside(source):
    """identify, with the lists of files read on the way, for the outer
    process to keep."""
    listings = {}

    def list_image(image):
        members = installer_image.list_archive(image)
        listings[str(Path(image).resolve())] = [[member.path, member.size] for member in members]
        return members

    try:
        answer = {"look": identify(source, list_image)}
    except InstallerProblem as problem:
        answer = {"problem": str(problem)}
    answer["listings"] = listings
    return answer


def import_installer(source, report):
    """The whole import: build in the sandbox (when there is one), then install."""
    work = cache_dir() / "import"
    staging = themes_dir() / ".import"
    check_space(work.parent, WORK_SPACE_BYTES, "the installer's parts")
    check_space(themes_dir(), THEME_SPACE_BYTES, "the themes")
    for folder in (work, staging):
        shutil.rmtree(folder, ignore_errors=True)
        folder.mkdir(parents=True)

    try:
        report.write_log(f"import of {source} into {themes_dir()}")
        if not sandbox_works():
            report.detail("No sandbox (bubblewrap is missing or cannot run here): "
                          "the installer is read without one")
        identified = identify_outside(source)
        if "problem" in identified:
            raise InstallerProblem(identified["problem"])
        look = read_looks()[identified["look"]]
        if not relay_inside(["build", source, work, staging, look["folder"]], [work, staging],
                            [source, listings_dir()], report):
            return False
        install(staging, look, report)
        updates.remember_installer(source)
    finally:
        shutil.rmtree(work, ignore_errors=True)
        shutil.rmtree(staging, ignore_errors=True)
    report.send(event="done")
    return True


def main():
    verbs = ("check", "identify", "identify-inside", "import", "build", "fingerprint", "fingerprint-inside",
             "news", "later")
    if len(sys.argv) < 2 or sys.argv[1] not in verbs:
        sys.exit(__doc__)
    command = sys.argv[1]
    if command == "check":
        try:
            looks = look_states()
        except updates.UpdatesProblem as problem:
            sys.exit(f"import_theme: {problem}")
        print(json.dumps(dict(missing_programs(), looks=looks, last_installer=updates.last_installer())))
        return
    if command == "news":
        print(json.dumps(news()))
        return
    if command == "later":
        if len(sys.argv) < 3:
            sys.exit("import_theme: later needs the looks to put off, e.g. `later sea-lion`")
        put_off(sys.argv[2:])
        return
    if command == "identify":
        print(json.dumps(identify_outside(sys.argv[2])))
        return
    if command == "identify-inside":
        print(json.dumps(identify_inside(sys.argv[2])))
        return
    if command == "fingerprint":
        work = cache_dir() / "fingerprint"
        check_space(work.parent, WORK_SPACE_BYTES, "the installer's parts")
        shutil.rmtree(work, ignore_errors=True)
        work.mkdir(parents=True)
        try:
            source = Path(sys.argv[2]).resolve()
            ok = run_inside(["fingerprint-inside", source, work], [work], readable=[source])
        finally:
            shutil.rmtree(work, ignore_errors=True)
        sys.exit(0 if ok else 1)
    if command == "fingerprint-inside":
        try:
            fingerprint(Path(sys.argv[2]), Path(sys.argv[3]))
        except InstallerProblem as problem:
            sys.exit(f"import_theme: {problem}")
        return

    # The window stops an import by ending this process.
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(1))
    try:
        if command == "build":
            # Inside the sandbox the log is not writable; the outer process keeps it.
            report = Report()
            source, work, staging, look_dir = map(Path, sys.argv[2:6])
            build(source, work, staging, look_dir, report)
        else:
            log_path().parent.mkdir(parents=True, exist_ok=True)
            report = Report(log_path())
            report.plan()
            if not import_installer(Path(sys.argv[2]).resolve(), report):
                sys.exit(1)
    except (InstallerProblem, updates.UpdatesProblem) as problem:
        report.send(event="error", message=str(problem))
        sys.exit(1)


if __name__ == "__main__":
    main()
