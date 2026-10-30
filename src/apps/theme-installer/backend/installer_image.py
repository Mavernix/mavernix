"""Reads the installer: finds its parts, checks them, and takes out
only the files the theme needs.

The installer comes as "Install OS X Mavericks.app", its InstallESD.dmg, or a
disk image of either. Inside are BaseSystem.dmg and Packages/NAME.pkg (xar
archives whose "Payload" is a bzip2-compressed cpio archive). 7-Zip reads the
whole of a disk image each time it opens one, so the parts are copied out
together, in one pass; each is then mined for the files in needed-files.txt
and deleted. Nothing from the installer is ever run.

The files taken are checked, not the parts around them: a part's bytes change
when it is signed anew, its files do not. A file the generators read by
position (DECODED_BY_POSITION) must be the one they were made for; any other
file may differ, and the theme is then made from what the installer has.
"""

import hashlib
import os
import re
import shutil
import subprocess
import threading
from dataclasses import dataclass
from pathlib import Path

# The parts of the installer the theme needs, by where they lie in it.
PARTS = {
    "BaseSystem": "BaseSystem.dmg",
    "BaseSystemBinaries": "Packages/BaseSystemBinaries.pkg",
    "Essentials": "Packages/Essentials.pkg",
    "AdditionalEssentials": "Packages/AdditionalEssentials.pkg",
    "MediaFiles": "Packages/MediaFiles.pkg",
}

# Files the decoders read by picture number or offset: in another version the
# same numbers would name other pictures. (CoreUI's colour table is found by
# its symbol's name, so another build of the library is read correctly; the
# library of 10.9.0, build 13A603, differs from 10.9.5's and gives the same
# 386 colours.)
DECODED_BY_POSITION = {
    "System/Library/PrivateFrameworks/CoreUI.framework/Versions/A/Resources/ArtFile.bin",
    "System/Library/PrivateFrameworks/CoreUI.framework/Versions/A/Resources/ArtFile200.bin",
    "System/Library/PrivateFrameworks/CoreUI.framework/Versions/A/Resources/SArtFile.bin",
    "System/Library/PrivateFrameworks/ProKit.framework/Versions/A/Resources/ProThemeBits.car",
    "Applications/iTunes.app/Contents/Resources/iTunes.rsrc",
    "Applications/iTunes.app/Contents/Resources/Images.rsrc",
    "Applications/iTunes.app/Contents/Resources/Images2.rsrc",
}

# Packages are installed over the base system, so the base system goes first.
PART_ORDER = ["BaseSystem", "BaseSystemBinaries", "Essentials", "AdditionalEssentials", "MediaFiles"]


# 7-Zip's switch that leaves out alternate streams: otherwise it writes a
# file's HFS+ extended attributes after its bytes to a pipe, and the checksum fails.
WITHOUT_STREAMS = "-sns-"

# lbzip2 uses every core (the largest payload is 3.2 GB); bunzip2 is the fallback.
def payload_decompressor():
    if shutil.which("lbzip2"):
        return ["lbzip2", "--decompress", "--stdout"]
    return ["bunzip2"]


# How much is read at a time when copying or checking a part.
CHUNK_BYTES = 4 * 1024 * 1024

# 7-Zip's progress for copying files out of an image: first "N% Open" while it
# reads the whole image, then "N% ..." while it writes the files.
SEVEN_ZIP_PROGRESS = re.compile(rb"^\s*(\d+)%( Open)?")

# Share of copying the parts spent opening the image, by the bytes each
# reads: the image (5.3 GB) and then the parts (4.4 GB).
OPEN_SHARE = 0.55


class InstallerProblem(Exception):
    """Something the user has to be told, in words they can act on."""


@dataclass
class Member:
    """A file inside an archive, as 7-Zip lists it."""

    path: str
    size: int


def list_archive(archive, archive_type=None):
    """Every file in an archive, with its size."""
    command = ["7z", "l", "-ba", "-slt", str(archive)]
    if archive_type:
        command.insert(2, f"-t{archive_type}")
    result = subprocess.run(command, capture_output=True, text=True, errors="surrogateescape")
    if result.returncode != 0:
        raise InstallerProblem(f"7-Zip cannot read {archive.name}: {result.stderr.strip() or 'unknown format'}")
    members, path, size = [], None, 0
    for line in result.stdout.split("\n") + [""]:
        if line.startswith("Path = "):
            path = line[len("Path = "):]
        elif line.startswith("Size = "):
            size = int(line[len("Size = "):] or 0)
        elif line == "" and path is not None:
            members.append(Member(path, size))
            path, size = None, 0
    return members


def needed_files(look_dir):
    """{part name: {path in the installed system: SHA-256}} from a look's needed-files.txt."""
    files = {}
    for line in (look_dir / "needed-files.txt").read_text().split("\n"):
        if line and not line.startswith("#"):
            part, checksum, path = line.split("\t", 2)
            files.setdefault(part, {})[path] = checksum
    return files


def file_checksum(path):
    digest = hashlib.sha256()
    with open(path, "rb") as file:
        for chunk in iter(lambda: file.read(CHUNK_BYTES), b""):
            digest.update(chunk)
    return digest.hexdigest()


def differing_files(files, root):
    """The files of {path: SHA-256} under `root` whose content is not the
    expected one. Refuses the installer if one of them is read by position."""
    differing = [path for path, checksum in files.items() if file_checksum(root / path) != checksum]
    decoded = [path for path in differing if path in DECODED_BY_POSITION]
    if decoded:
        raise InstallerProblem(
            "This is not the OS X 10.9.5 installer the Theme Installer is made for: "
            f"{Path(decoded[0]).name} differs from the one in that version.")
    return differing


def find_installer(source, work, report, known_listing=None):
    """Where the installer's parts are: (the archive holding them, {part:
    Member}). An archive that holds InstallESD.dmg only, as the .app does
    or a disk image made of the .app, is opened one level deeper.

    `known_listing(path)` gives an image's list of files when it was listed
    before (or None), saving a full read of it."""
    source = Path(source)
    if source.is_dir():
        inside_app = source / "Contents" / "SharedSupport" / "InstallESD.dmg"
        if not inside_app.is_file():
            raise InstallerProblem(f"{source.name} is a folder, but not Apple's installer program: "
                               "it has no Contents/SharedSupport/InstallESD.dmg.")
        source = inside_app

    members = known_listing(source) if known_listing else None
    if members is None:
        members = list_archive(source)
    parts = match_parts(members)
    if parts:
        return source, parts

    nested = [member for member in members if member.path.endswith("InstallESD.dmg")]
    if not nested:
        raise InstallerProblem(f"{source.name} does not contain OS X 10.9's installer: "
                           "there is no BaseSystem.dmg and no Packages folder in it.")
    report.detail("The installer is wrapped in another image; taking it out first.")
    inner = copy_member(source, nested[0], work / "InstallESD.dmg", report)
    parts = match_parts(list_archive(inner))
    if not parts:
        raise InstallerProblem("The InstallESD.dmg inside the image does not contain the installer's parts.")
    return inner, parts


def match_parts(members):
    """{part name: Member} for every part found, or {} if any is missing.
    Parts are recognised by the end of their path, so that any volume name
    or folder around them does not matter."""
    found = {}
    for name, suffix in PARTS.items():
        for member in members:
            if member.path == suffix or member.path.endswith("/" + suffix):
                found[name] = member
                break
    return found if len(found) == len(PARTS) else {}


def copy_member(archive, member, destination, report):
    """Copies one file out of an archive, reporting how far it has come."""
    destination.parent.mkdir(parents=True, exist_ok=True)
    with open(destination, "wb") as output:
        reader = subprocess.Popen(["7z", "x", WITHOUT_STREAMS, "-so", str(archive), member.path],
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        copied = 0
        for chunk in iter(lambda: reader.stdout.read(CHUNK_BYTES), b""):
            output.write(chunk)
            copied += len(chunk)
            report.fraction(copied / max(member.size, 1))
        errors = reader.stderr.read().decode(errors="replace")
        if reader.wait() != 0:
            raise InstallerProblem(f"{archive.name} is damaged or incomplete: 7-Zip could not copy {member.path} "
                               f"out of it ({errors.strip()}). Download the installer again.")
    return destination


def copy_members(archive, members, destination, report):
    """Copies files out of an archive in one pass into `destination`, by their
    paths in it; returns {member path: copied file}."""
    destination.mkdir(parents=True, exist_ok=True)
    reader = subprocess.Popen(
        ["7z", "x", "-y", WITHOUT_STREAMS, "-bsp1", "-bso0", "-bse2", f"-o{destination}", str(archive),
         *[member.path for member in members]],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    errors = []
    reading_errors = threading.Thread(target=lambda: errors.append(reader.stderr.read()))
    reading_errors.start()

    # The progress line is rewritten in place with backspaces.
    unread = b""
    while chunk := os.read(reader.stdout.fileno(), 4096):
        *pieces, unread = re.split(rb"[\b\r\n]+", unread + chunk)
        for piece in pieces:
            found = SEVEN_ZIP_PROGRESS.match(piece)
            if not found:
                continue
            fraction = int(found.group(1)) / 100
            report.fraction(fraction * OPEN_SHARE if found.group(2) else OPEN_SHARE + fraction * (1 - OPEN_SHARE))
    reading_errors.join()
    message = b"".join(errors).decode(errors="replace").strip()
    if reader.wait() != 0:
        raise InstallerProblem(f"{archive.name} is damaged or incomplete: 7-Zip could not copy the installer's "
                               f"parts out of it ({message or 'no reason given'}). Download the installer again.")

    copied = {}
    for member in members:
        path = destination / member.path
        if not path.is_file() or path.stat().st_size != member.size:
            raise InstallerProblem(f"{archive.name} is damaged or incomplete: {member.path} did not come out whole. "
                                   "Download the installer again.")
        copied[member.path] = path
    return copied


def take_files_from_base_system(image, paths, root, report):
    """Copies the listed files out of BaseSystem.dmg into root."""
    members = list_archive(image)
    by_installed_path = {}
    for member in members:
        # Paths start with the volume's name ("OS X Base System/...").
        _volume, _, installed_path = member.path.partition("/")
        by_installed_path[installed_path] = member.path
    missing = [path for path in paths if path not in by_installed_path]
    if missing:
        raise InstallerProblem(f"BaseSystem.dmg lacks files the theme needs, such as {missing[0]}.")

    extract_to = root.parent / "base-system"
    list_file = root.parent / "base-system-files.txt"
    list_file.write_text("\n".join(by_installed_path[path] for path in paths) + "\n")
    result = subprocess.run(["7z", "x", "-y", WITHOUT_STREAMS, f"-o{extract_to}", str(image), f"@{list_file}"],
                            capture_output=True, text=True)
    if result.returncode != 0:
        raise InstallerProblem(f"BaseSystem.dmg is damaged or incomplete: 7-Zip could not take files out of it "
                               f"({result.stderr.strip()}). Download the installer again.")
    for number, path in enumerate(paths, start=1):
        source = extract_to / by_installed_path[path]
        target = root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.move(source, target)
        report.fraction(number / len(paths))
    shutil.rmtree(extract_to)
    list_file.unlink()


def take_files_from_package(package, paths, root, report):
    """Copies the listed files out of a package's payload into root.

    7-Zip, the decompressor and cpio are joined by pipes, so the payload is
    never written out whole.
    """
    payload = next((member for member in list_archive(package, "xar") if member.path == "Payload"), None)
    if payload is None:
        raise InstallerProblem(f"{package.name} has no payload.")

    read_payload = subprocess.Popen(["7z", "x", "-txar", WITHOUT_STREAMS, "-so", str(package), "Payload"],
                                    stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    decompress = subprocess.Popen(payload_decompressor(), stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    unpack = subprocess.Popen(
        ["cpio", "--extract", "--make-directories", "--no-absolute-filenames", "--quiet",
         *[f"./{path}" for path in paths]],
        stdin=decompress.stdout, cwd=root, stderr=subprocess.PIPE)
    decompress.stdout.close()

    # Passed on here rather than piped directly, so the bytes can be counted.
    def pass_payload_on():
        passed = 0
        for chunk in iter(lambda: read_payload.stdout.read(CHUNK_BYTES), b""):
            decompress.stdin.write(chunk)
            passed += len(chunk)
            report.fraction(passed / max(payload.size, 1))
        decompress.stdin.close()

    passing = threading.Thread(target=pass_payload_on)
    passing.start()
    cpio_messages = unpack.communicate()[1].decode(errors="replace")
    passing.join()
    read_payload.wait()
    if decompress.wait() != 0:
        raise InstallerProblem(f"{package.name} is damaged or incomplete: its contents did not decompress "
                               "cleanly. Download the installer again.")

    missing = [path for path in paths if not (root / path).exists()]
    if missing:
        raise InstallerProblem(f"{package.name} lacks files the theme needs, such as {missing[0]}. "
                           f"{cpio_messages.strip()}")
