"""Reads the table of numbered colours built into the CoreUI framework.

Usage:

    python coreui_colors.py ROOT/System/Library/PrivateFrameworks/CoreUI.framework/Versions/A/CoreUI \\
        WORK/coreui-colors.json

CoreUI draws some parts (window title bars among them) from colours it looks
up by number. Colours 0 to 231 are in the generic RGB table, 232 to 385 in the
sRGB table; each entry is four doubles, red, green, blue, alpha, from 0 to 1.
The file is only read, never run.
"""

import argparse
import json
import struct
import sys
from pathlib import Path

from macho import MachO

# Names of the two tables in CoreUI's symbol table.
GENERIC_RGB_TABLE = "__CUIColorGetComponents.kGenericRGBColorTable"
SRGB_TABLE = "__CUIColorGetComponents.kSRGBColorTable"

HIGHEST_COLOR_NUMBER = 0x181
# Numbers from here on are in the sRGB table.
FIRST_SRGB_NUMBER = 0xE8

ENTRY = struct.Struct("<4d")  # red, green, blue, alpha


def fail(message):
    sys.exit(f"coreui_colors: {message}")


def address_of(binary, symbol_name):
    for symbol in binary.symbols:
        if symbol.name == symbol_name:
            return symbol.address
    fail(f"{symbol_name} not found. Is this CoreUI from the supported installer?")


def to_hex(red, green, blue):
    """Colour as #rrggbb, rounding each channel to the nearest of 256 steps."""
    return "#" + "".join(f"{round(channel * 255):02x}" for channel in (red, green, blue))


def read_colors(binary):
    generic_table = address_of(binary, GENERIC_RGB_TABLE)
    srgb_table = address_of(binary, SRGB_TABLE)

    colors = []
    for number in range(HIGHEST_COLOR_NUMBER + 1):
        if number < FIRST_SRGB_NUMBER:
            entry_address = generic_table + number * ENTRY.size
            color_space = "generic RGB"
        else:
            entry_address = srgb_table + (number - FIRST_SRGB_NUMBER) * ENTRY.size
            color_space = "sRGB"

        red, green, blue, alpha = ENTRY.unpack(binary.bytes_at(entry_address, ENTRY.size))
        colors.append(
            {
                "number": number,
                "color_space": color_space,
                "red": red,
                "green": green,
                "blue": blue,
                "alpha": alpha,
                # For reading by eye; the exact values above are what counts.
                "hex": to_hex(red, green, blue),
            }
        )
    return colors


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("coreui", type=Path, help="the CoreUI framework's program file")
    parser.add_argument("output", type=Path, help="JSON file to write")
    arguments = parser.parse_args()

    colors = read_colors(MachO(arguments.coreui))
    with open(arguments.output, "w") as output:
        json.dump({"source": str(arguments.coreui), "colors": colors}, output, indent=1)
    print(f"{len(colors)} colours written to {arguments.output}")


if __name__ == "__main__":
    main()
