"""Makes a look's themes from an installer the user owns.

    python import_theme.py check                what is missing to import, and
                                                the looks with their state, as JSON
    python import_theme.py identify SOURCE      which look SOURCE makes, as JSON
    python import_theme.py import SOURCE        the whole import
    python import_theme.py fingerprint SOURCE   the SHA-256 of every file the
                                                look needs, as needed-files.txt
                                                lists them (to add another
                                                version of the installer)

Each look is a folder in looks/ (look.toml says what is in it). SOURCE is
whatever its look.toml recognises: for Sea Lion, "Install OS X Mavericks.app",
its InstallESD.dmg, or a disk image made from either. The import writes the
look's themes into ~/.local/share/mavernix/themes/ and its desktops into
~/.local/share/mavernix/config/, where the session finds them.

What it is doing is reported on standard output, one JSON object per line,
for the import window to show (see Report below). The work that reads the
installer runs in a sandbox when bubblewrap is installed: no network, and the
whole file system read-only except the import's own two folders.
"""

import json
import os
import shutil
import signal
import subprocess
import sys
import time
import tomllib
from pathlib import Path

import installer_image
from installer_image import InstallerProblem

BACKEND_DIR = Path(__file__).resolve().parent

LOOKS_DIR = BACKEND_DIR / "looks"

# Share of each part's unpacking time spent copying it out; the rest is
# taking the files, which decompresses the whole payload.
COPY_SHARE = 0.3

# The steps of an import, as the window lists them, with a rough share of the
# whole time each takes.
STAGES = [
    ("open", "Opening the installer", 1),
    ("unpack", "Checking and unpacking the installer", 70),
    ("decode", "Reading the artwork", 8),
    ("theme", "Building the theme", 6),
    ("icons", "Converting the icons", 8),
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

# Free space needed: the largest installer part (3.2 GB) with room to unpack
# it, and the finished themes.
WORK_SPACE_BYTES = 4_500_000_000
THEME_SPACE_BYTES = 500_000_000

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
        if self.log and event["event"] in ("stage", "detail", "note", "error", "done"):
            self.log.write(f"{time.strftime('%H:%M:%S')} {json.dumps(event)}\n")
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
    """What the window lists: each look, and whether its themes are installed."""
    states = []
    for look_id, look in read_looks().items():
        installed = all((themes_dir() / theme / "theme.toml").is_file() for theme in look["themes"])
        states.append({"id": look_id, "name": look["name"], "description": look["description"],
                       "source": look["source"], "drop_hint": look["drop_hint"],
                       "licence_note": look.get("licence_note", ""), "desktop": look["desktops"][0],
                       "installed": installed})
    return states


def holds(paths, suffix):
    """Whether an archive's list of paths has a file ending in `suffix`,
    whatever volume or folder is around it."""
    return any(path == suffix or path.endswith("/" + suffix) for path in paths)


def identify(source):
    """The id of the look whose installer SOURCE is. Reads only the image's
    list of files, so it answers within a second or two."""
    source = Path(source)
    for look_id, look in read_looks().items():
        image = source / look["inside_program"] if source.is_dir() else source
        if not image.is_file():
            continue
        try:
            paths = [member.path for member in installer_image.list_archive(image)]
        except InstallerProblem:
            continue
        if all(holds(paths, suffix) for suffix in look["recognise"]) or holds(paths, look["wrapped_in"]):
            return look_id
    raise InstallerProblem(f"{source.name} is not an installer the Theme Installer knows. "
                           "The looks it can make, and what each is made from, are on its first page.")


def user_folder(variable, fallback):
    return Path(os.environ.get(variable) or Path.home() / fallback)


def themes_dir():
    return user_folder("XDG_DATA_HOME", ".local/share") / "mavernix" / "themes"


def run_tool(tool, *arguments):
    """Runs one of the decoders or generators beside this file."""
    result = subprocess.run([sys.executable, str(BACKEND_DIR / tool), *map(str, arguments)],
                            capture_output=True, text=True, cwd=BACKEND_DIR)
    if result.returncode != 0:
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
    done = 0
    differing = []
    for name in installer_image.PART_ORDER:
        share = sizes[name] / total
        start = done / total
        part_file = work / Path(parts[name].path).name
        report.detail(f"Copying {part_file.name}")
        report.within(start, start + share * COPY_SHARE)
        installer_image.copy_member(archive, parts[name], part_file, report)
        report.detail(f"Taking the theme's files from {part_file.name}")
        report.within(start + share * COPY_SHARE, start + share)
        if name == "BaseSystem":
            installer_image.take_files_from_base_system(part_file, list(files[name]), root, report)
        else:
            installer_image.take_files_from_package(part_file, list(files[name]), root, report)
        part_file.unlink()
        if check:
            differing += installer_image.differing_files(files[name], root)
        done += sizes[name]
    nested = work / "InstallESD.dmg"
    if nested.exists():
        nested.unlink()
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
    archive, parts = installer_image.find_installer(source, work, report)
    report.fraction(1)

    report.stage("unpack")
    root = unpack(archive, parts, work, look_dir, report, check=True)

    report.stage("decode")
    frameworks = root / "System/Library/PrivateFrameworks"
    coreui = frameworks / "CoreUI.framework/Versions/A"
    artwork = work / "artwork"
    decoders = [
        ("the controls' artwork", "artfile.py", coreui / "Resources/ArtFile.bin", artwork / "artfile-1x"),
        ("the controls' artwork at double density", "artfile.py", coreui / "Resources/ArtFile200.bin", artwork / "artfile-2x"),
        ("the shared artwork", "sartfile.py", coreui / "Resources/SArtFile.bin", artwork / "sartfile"),
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


def sandboxed(command, writable):
    """`command` inside bubblewrap: the file system read-only except the
    folders in `writable`, no network, and ended when this process ends."""
    sandbox = ["bwrap", "--ro-bind", "/", "/", "--dev", "/dev", "--proc", "/proc", "--tmpfs", "/tmp"]
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


def run_inside(arguments, writable, capture=False):
    """Runs this program with `arguments`, in the sandbox when there is one;
    true if it succeeded. With `capture`, returns its output instead (or None)."""
    command = [sys.executable, str(Path(__file__).resolve())] + [str(argument) for argument in arguments]
    if sandbox_works():
        command = sandboxed(command, writable)
    result = subprocess.run(command, capture_output=capture, text=capture)
    if capture:
        return result.stdout if result.returncode == 0 else None
    return result.returncode == 0


def identify_outside(source):
    """{"look": id} or {"problem": message} for SOURCE, worked out in the sandbox."""
    answer = run_inside(["identify-inside", Path(source).resolve()], [], capture=True)
    if answer is None:
        return {"problem": "The Theme Installer could not read the file."}
    return json.loads(answer)


def import_installer(source, report):
    """The whole import: build in the sandbox (when there is one), then install."""
    work = user_folder("XDG_CACHE_HOME", ".cache") / "mavernix" / "import"
    staging = themes_dir() / ".import"
    check_space(work.parent, WORK_SPACE_BYTES, "the installer's parts")
    check_space(themes_dir(), THEME_SPACE_BYTES, "the themes")
    for folder in (work, staging):
        shutil.rmtree(folder, ignore_errors=True)
        folder.mkdir(parents=True)

    try:
        if not sandbox_works():
            report.detail("No sandbox (bubblewrap is missing or cannot run here): "
                          "the installer is read without one")
        identified = identify_outside(source)
        if "problem" in identified:
            raise InstallerProblem(identified["problem"])
        look = read_looks()[identified["look"]]
        if not run_inside(["build", source, work, staging, look["folder"]], [work, staging]):
            return False
        install(staging, look, report)
    finally:
        shutil.rmtree(work, ignore_errors=True)
        shutil.rmtree(staging, ignore_errors=True)
    report.send(event="done")
    return True


def main():
    verbs = ("check", "identify", "identify-inside", "import", "build", "fingerprint", "fingerprint-inside")
    if len(sys.argv) < 2 or sys.argv[1] not in verbs:
        sys.exit(__doc__)
    command = sys.argv[1]
    if command == "check":
        print(json.dumps(dict(missing_programs(), looks=look_states())))
        return
    if command == "identify":
        print(json.dumps(identify_outside(sys.argv[2])))
        return
    if command == "identify-inside":
        try:
            print(json.dumps({"look": identify(sys.argv[2])}))
        except InstallerProblem as problem:
            print(json.dumps({"problem": str(problem)}))
        return
    if command == "fingerprint":
        work = user_folder("XDG_CACHE_HOME", ".cache") / "mavernix" / "fingerprint"
        check_space(work.parent, WORK_SPACE_BYTES, "the installer's parts")
        shutil.rmtree(work, ignore_errors=True)
        work.mkdir(parents=True)
        try:
            ok = run_inside(["fingerprint-inside", Path(sys.argv[2]).resolve(), work], [work])
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
            log = user_folder("XDG_CACHE_HOME", ".cache") / "mavernix" / "theme-installer.log"
            log.parent.mkdir(parents=True, exist_ok=True)
            report = Report(log)
            report.plan()
            if not import_installer(Path(sys.argv[2]), report):
                sys.exit(1)
    except InstallerProblem as problem:
        report.send(event="error", message=str(problem))
        sys.exit(1)


if __name__ == "__main__":
    main()
