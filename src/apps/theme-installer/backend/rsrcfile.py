"""Decodes a classic Mac resource file, such as iTunes' Images.rsrc.

Usage:

    python rsrcfile.py ROOT/Applications/iTunes.app/Contents/Resources/iTunes.rsrc \\
        WORK/artwork/itunes/iTunes

Writes each "PNG " resource as NUMBER.png and an index.json. iTunes 11 keeps
its pictures this way, numbered and unnamed: iTunes.rsrc at normal size, and
Images.rsrc and Images2.rsrc at double size, each numbered 15000 above its
normal-size twin. The layout is Inside Macintosh's (More Macintosh Toolbox,
1-121); all numbers are big-endian.

    Header         offset of the data area (u32), offset of the map (u32),
                   length of the data area (u32), length of the map (u32)
    Data area      for each resource: its length (u32), then its bytes
    Map            a copy of the header and 8 bytes of the system's, then
                   attributes (u16), offset of the type list from the map
                   (u16), offset of the name list from the map (u16)
    Type list      number of types minus one (u16), then per type: its code
                   (4 bytes), number of resources minus one (u16), offset of
                   its reference list from the type list (u16)
    Reference      number (s16), offset of its name in the name list or
                   0xffff (u16), attributes (u8) and offset of its data in
                   the data area (u24), 4 bytes the system used (u32)
"""

import argparse
import json
import struct
import sys
from pathlib import Path

HEADER = struct.Struct(">IIII")
# After the header's copy (16 bytes), the system's handle (4) and file
# number (2), and the attributes (2).
MAP_LISTS_AT = 24
MAP_LISTS = struct.Struct(">HH")
TYPE_ENTRY = struct.Struct(">4sHH")
REFERENCE = struct.Struct(">hHI4x")
LENGTH = struct.Struct(">I")

PICTURE_TYPE = b"PNG "
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def fail(message):
    sys.exit(f"rsrcfile: {message}")


def resources(data, wanted_type):
    """Yields (number, bytes) for every resource of type `wanted_type`."""
    data_offset, map_offset, _, map_length = HEADER.unpack_from(data, 0)
    if map_offset + map_length > len(data):
        fail("the map lies past the end of the file; is it a resource file?")

    type_list_offset, _ = MAP_LISTS.unpack_from(data, map_offset + MAP_LISTS_AT)
    type_list = map_offset + type_list_offset
    (type_count_less_one,) = struct.unpack_from(">H", data, type_list)

    for type_number in range(type_count_less_one + 1):
        code, count_less_one, references_offset = TYPE_ENTRY.unpack_from(data, type_list + 2 + type_number * TYPE_ENTRY.size)
        if code != wanted_type:
            continue
        for position in range(count_less_one + 1):
            number, _, attributes_and_offset = REFERENCE.unpack_from(
                data, type_list + references_offset + position * REFERENCE.size)
            start = data_offset + (attributes_and_offset & 0xffffff)
            (length,) = LENGTH.unpack_from(data, start)
            yield number, data[start + LENGTH.size:start + LENGTH.size + length]


def decode(rsrc_file_path, output_dir):
    """Writes every picture plus index.json. Returns the number of pictures."""
    data = rsrc_file_path.read_bytes()
    output_dir.mkdir(parents=True, exist_ok=True)
    numbers = []
    # iTunes.rsrc has one "PNG " resource (4690) that is a Photoshop file.
    not_png = []

    for number, picture_bytes in resources(data, PICTURE_TYPE):
        if not picture_bytes.startswith(PNG_SIGNATURE):
            not_png.append(number)
            continue
        (output_dir / f"{number}.png").write_bytes(picture_bytes)
        numbers.append(number)

    with open(output_dir / "index.json", "w") as index_file:
        json.dump({"source": rsrc_file_path.name, "pictures": sorted(numbers), "not_png": sorted(not_png)},
                  index_file, indent=1)

    return len(numbers)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("rsrc_file", type=Path, help="the resource file, such as Images.rsrc")
    parser.add_argument("output", type=Path, help="directory for the pictures and index.json")
    arguments = parser.parse_args()

    if not arguments.rsrc_file.is_file():
        fail(f"{arguments.rsrc_file} does not exist")

    picture_count = decode(arguments.rsrc_file, arguments.output)
    print(f"{arguments.rsrc_file.name}: {picture_count} pictures written to {arguments.output}")


if __name__ == "__main__":
    main()
