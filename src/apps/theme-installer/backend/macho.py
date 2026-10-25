"""Reads the structure of a 64-bit Mach-O file: sections and symbols.

Parses only headers and tables, so that a named piece of data can be found
and read; nothing is executed. Only 64-bit Intel code is supported; of a
"fat" file, the 64-bit part is taken.
"""

import struct
from dataclasses import dataclass

MAGIC_64 = 0xFEEDFACF

# A fat file starts with this number, stored big-endian (unlike the rest).
FAT_MAGIC = 0xCAFEBABE
CPU_TYPE_X86_64 = 0x01000007
LC_SEGMENT_64 = 0x19
LC_SYMTAB = 0x2

# Section type (the low byte of a section's flags): no bytes in the file;
# memory starts as zeros.
SECTION_TYPE_ZERO_FILL = 0x1


@dataclass
class Section:
    segment: str
    name: str
    address: int       # where the section sits in memory when loaded
    size: int
    file_offset: int   # where its bytes sit in the file
    kind: int = 0      # the section type, e.g. SECTION_TYPE_ZERO_FILL


@dataclass
class Symbol:
    name: str
    address: int


class MachO:
    def __init__(self, path):
        self.path = path
        with open(path, "rb") as file:
            self.data = self._take_64_bit_part(file.read())

        magic = struct.unpack_from("<I", self.data, 0)[0]
        if magic != MAGIC_64:
            raise ValueError(f"{path} is not a 64-bit little-endian Mach-O file")

        self.sections = []
        self.symbols = []
        self._read_load_commands()

    @staticmethod
    def _take_64_bit_part(file_bytes):
        """Returns the 64-bit Intel part of a fat file, or the file as it is.

        Offsets inside a Mach-O count from its own start, so cutting it out is safe.
        """
        if struct.unpack_from(">I", file_bytes, 0)[0] != FAT_MAGIC:
            return file_bytes

        part_count = struct.unpack_from(">I", file_bytes, 4)[0]
        for position in range(part_count):
            # Directory entry: cpu type, cpu subtype, offset, size, alignment.
            cpu_type, _, offset, size, _ = struct.unpack_from(">IIIII", file_bytes, 8 + position * 20)
            if cpu_type == CPU_TYPE_X86_64:
                return file_bytes[offset:offset + size]
        raise ValueError("fat file has no 64-bit Intel part")

    def _read_load_commands(self):
        command_count = struct.unpack_from("<I", self.data, 16)[0]
        offset = 32  # size of the 64-bit Mach-O header
        for _ in range(command_count):
            command, command_size = struct.unpack_from("<II", self.data, offset)
            if command == LC_SEGMENT_64:
                self._read_segment(offset)
            elif command == LC_SYMTAB:
                self._read_symbols(offset)
            offset += command_size

    def _read_segment(self, offset):
        segment_name = self.data[offset + 8:offset + 24].rstrip(b"\0").decode()
        section_count = struct.unpack_from("<I", self.data, offset + 64)[0]

        # Section headers follow the 72-byte segment command, 80 bytes each.
        for position in range(section_count):
            start = offset + 72 + position * 80
            name = self.data[start:start + 16].rstrip(b"\0").decode()
            address, size, file_offset = struct.unpack_from("<QQI", self.data, start + 32)
            section_flags = struct.unpack_from("<I", self.data, start + 64)[0]
            kind = section_flags & 0xFF
            self.sections.append(Section(segment_name, name, address, size, file_offset, kind))

    def _read_symbols(self, offset):
        table_offset, symbol_count, strings_offset, _ = struct.unpack_from(
            "<IIII", self.data, offset + 8
        )
        # Each symbol entry is 16 bytes: name offset (u32), type (u8),
        # section (u8), description (u16), address (u64).
        for position in range(symbol_count):
            name_offset, symbol_type, _, _, address = struct.unpack_from(
                "<IBBHQ", self.data, table_offset + position * 16
            )

            # Skip debugging entries and symbols defined in other libraries.
            is_debug_entry = symbol_type & 0xE0
            is_defined_here = (symbol_type & 0x0E) == 0x0E
            if is_debug_entry or not is_defined_here:
                continue

            name_start = strings_offset + name_offset
            name_end = self.data.index(b"\0", name_start)
            name = self.data[name_start:name_end].decode("ascii", "replace")
            self.symbols.append(Symbol(name, address))
        self.symbols.sort(key=lambda symbol: symbol.address)

    def section_containing(self, address):
        for section in self.sections:
            if section.address <= address < section.address + section.size:
                return section
        return None

    def bytes_at(self, address, length):
        """Returns `length` bytes as they would be in memory at `address`."""
        section = self.section_containing(address)
        if section is None:
            raise ValueError(f"address {address:#x} is not inside any section")
        if section.kind == SECTION_TYPE_ZERO_FILL:
            raise ValueError(f"section {section.name} has no bytes in the file; it starts as zeros")
        start = section.file_offset + (address - section.address)
        return self.data[start:start + length]
