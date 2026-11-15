#!/usr/bin/env python3
"""
Patch DT_NEEDED / DT_SONAME strings inside a 32-bit ARM ELF file.

Why this exists
---------------
The Qualcomm CNE blobs shipped in the stock ROM (libcne.so,
libcneapiclient.so, cnd, mediadrm/libwvdrmengine.so) were compiled against
protobuf 3.5.  LineageOS 15.1 ships protobuf 3.0.0-beta-3 (external/protobuf),
which is missing several symbols those blobs import:

  google::protobuf::internal::StringTypeHandlerBase::New()
  google::protobuf::internal::StringTypeHandlerBase::Delete(std::string*)
  google::protobuf::io::CodedInputStream::ReadVarint32Fallback(unsigned int*)
  google::protobuf::io::CodedInputStream::ReadVarint64Fallback(unsigned long*)
  google::protobuf::io::CodedInputStream::ReadTagFallback()
  google::protobuf::io::CodedOutputStream::WriteVarint32(unsigned int)
  google::protobuf::internal::WireFormatLite::ReadString(...)
  google::protobuf::internal::RepeatedPtrFieldBase::Swap(...)

The stock ROM's own libprotobuf-cpp-lite.so *does* export all of them, but it
cannot simply replace the LineageOS one: libhwui.so, libsurfaceflinger.so,
libtextclassifier.so, libmediadrm.so, libkeystore_binder.so,
bluetooth.default.so, perfprofd and libwebrtc_audio_preprocessing.so need
symbols that only exist in the 3.0 runtime, so both runtimes have to coexist.

Therefore the stock runtime is installed under a second name and the CNE blobs
are pointed at it with a DT_NEEDED rewrite.  This script performs that rewrite.

Only string *replacements that are not longer* than the original are done, so
the .dynstr section never has to grow and no segment/section header needs to be
adjusted -- the patched file stays byte-compatible with the original layout.

Usage:
    patch-elf-dynstr.py <file> --replace <old>=<new> [--replace <old>=<new> ...]
"""

import argparse
import struct
import sys

# ELF constants
ELFMAG = b"\x7fELF"
ELFCLASS32 = 1
ELFCLASS64 = 2
ELFDATA2LSB = 1
SHT_DYNAMIC = 6
DT_NULL = 0
DT_NEEDED = 1
DT_STRTAB = 5
DT_SONAME = 14


class ElfError(Exception):
    pass


class Elf:
    """Minimal ELF32/ELF64 reader/writer good enough for .dynamic surgery."""

    def __init__(self, data):
        self.data = bytearray(data)
        if self.data[:4] != ELFMAG:
            raise ElfError("not an ELF file")
        self.ei_class = self.data[4]
        if self.ei_class not in (ELFCLASS32, ELFCLASS64):
            raise ElfError("unsupported ELF class %d" % self.ei_class)
        self.is64 = self.ei_class == ELFCLASS64
        self.le = self.data[5] == ELFDATA2LSB
        end = "<" if self.le else ">"

        if self.is64:
            (e_type, e_machine, _e_version, e_entry, e_phoff, e_shoff,
             e_flags, e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum,
             e_shstrndx) = struct.unpack_from(end + "HHIQQQIHHHHHH", self.data, 16)
        else:
            (e_type, e_machine, _e_version, e_entry, e_phoff, e_shoff,
             e_flags, e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum,
             e_shstrndx) = struct.unpack_from(end + "HHIIIIIHHHHHH", self.data, 16)
        self.e_phoff, self.e_phnum, self.e_phentsize = e_phoff, e_phnum, e_phentsize
        self.e_shoff, self.e_shnum, self.e_shentsize = e_shoff, e_shnum, e_shentsize
        self.end = end

        # section headers
        self.sections = []
        for i in range(e_shnum):
            off = e_shoff + i * e_shentsize
            if self.is64:
                (sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size,
                 sh_link, sh_info, sh_addralign, sh_entsize) = struct.unpack_from(
                    end + "IIQQQQIIQQ", self.data, off)
            else:
                (sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size,
                 sh_link, sh_info, sh_addralign, sh_entsize) = struct.unpack_from(
                    end + "IIIIIIIIII", self.data, off)
            self.sections.append(dict(name=sh_name, type=sh_type, offset=sh_offset,
                                      size=sh_size, link=sh_link, entsize=sh_entsize,
                                      addr=sh_addr))

        self.dynstr = None
        self.dynamic = None
        for sec in self.sections:
            if sec["type"] == SHT_DYNAMIC:
                self.dynamic = sec
            if sec["type"] == 3 and self.dynstr is None:  # SHT_STRTAB
                self.dynstr = sec
        if self.dynamic is None or self.dynstr is None:
            raise ElfError("no .dynamic/.dynstr section found")

    # -- string table helpers -------------------------------------------
    def strtab(self, offset):
        """Return the NUL terminated string at `offset` in .dynstr."""
        end = self.data.index(b"\x00", self.dynstr["offset"] + offset)
        return self.data[self.dynstr["offset"] + offset:end].decode("utf-8", "replace")

    def write_strtab(self, offset, new):
        """Overwrite the string at `offset` in .dynstr (must not grow)."""
        start = self.dynstr["offset"] + offset
        end = self.data.index(b"\x00", start)
        raw = new.encode("utf-8")
        if len(raw) > end - start:
            raise ElfError("replacement %r does not fit in %d bytes" %
                           (new, end - start))
        # Keep the slot exactly the same size: a plain slice assignment with a
        # shorter string would shrink the bytearray and truncate the file.
        self.data[start:end] = raw + b"\x00" * (end - start - len(raw))
        # keep the terminator
        self.data[end] = 0

    # -- dynamic entries -------------------------------------------------
    def dynamic_entries(self):
        """Yield (d_tag, d_val) for every entry of .dynamic."""
        sec = self.dynamic
        entsize = sec["entsize"] or (16 if self.is64 else 8)
        fmt = self.end + ("qQ" if self.is64 else "iI")
        off = sec["offset"]
        while off + entsize <= sec["offset"] + sec["size"]:
            d_tag, d_val = struct.unpack_from(fmt, self.data, off)
            if d_tag == DT_NULL:
                break
            yield d_tag, d_val, off
            off += entsize

    def needed(self):
        out = []
        for d_tag, d_val, _off in self.dynamic_entries():
            if d_tag == DT_NEEDED:
                out.append(d_val)
        return out

    def soname(self):
        for d_tag, d_val, _off in self.dynamic_entries():
            if d_tag == DT_SONAME:
                return d_val
        return None

    def replace_needed(self, old, new):
        """Re-point one DT_NEEDED entry at a different library.

        The string is patched in place, so the new name must not be longer
        than the old one: the string table keeps its size and no symbol
        offset moves.

        Returns the number of entries that were changed (0 or 1).
        """
        hits = [d_val for d_tag, d_val, _off in self.dynamic_entries()
                if d_tag == DT_NEEDED and self.strtab(d_val) == old]
        if not hits:
            return 0
        if len(hits) > 1:
            raise ElfError("%r is needed %d times - refusing to guess" %
                           (old, len(hits)))
        self.write_strtab(hits[0], new)
        return 1

    def patch_string(self, old, new):
        """Replace every occurrence of `old` in .dynstr with `new`.

        Returns the number of occurrences replaced.  All occurrences are
        patched (the same .dynstr string may be shared by several entries).
        """
        count = 0
        base = self.dynstr["offset"]
        limit = base + self.dynstr["size"]
        pos = base
        while pos < limit:
            idx = self.data.find(b"\x00", pos, limit)
            if idx < 0:
                break
            s = self.data[pos:idx]
            if s == old.encode("utf-8"):
                if len(new) > len(s):
                    raise ElfError(
                        "replacement %r is longer than %r - refusing" % (new, old))
                # Keep the slot the same size - assigning a shorter byte string
                # to a slice would shrink the file.
                raw = new.encode("utf-8")
                self.data[pos:idx] = raw + b"\x00" * (len(s) - len(raw))
                count += 1
            pos = idx + 1
        return count

    def rename_substring(self, old, new):
        """Replace `old` with `new` inside every symbol name in .dynstr.

        Both strings must have the same length: the rename has to keep the
        string table exactly the same size, otherwise every symbol offset
        after the renamed one would shift.

        Only strings that look like a mangled symbol name (they start with
        "_Z") are touched, so that a stray occurrence inside a library name
        cannot corrupt it.

        Returns the number of symbol names that were changed.
        """
        if len(old) != len(new):
            raise ElfError("rename %r -> %r changes the length" % (old, new))
        old_b, new_b = old.encode("utf-8"), new.encode("utf-8")
        count = 0
        base = self.dynstr["offset"]
        limit = base + self.dynstr["size"]
        pos = base
        while pos < limit:
            idx = self.data.find(b"\x00", pos, limit)
            if idx < 0:
                break
            s = bytes(self.data[pos:idx])
            if s.startswith(b"_Z") and old_b in s:
                # Same length -> the slot keeps its size, no offset shifts.
                self.data[pos:idx] = s.replace(old_b, new_b)
                count += 1
            pos = idx + 1
        return count

    def has_substring(self, needle):
        """True if any symbol name in .dynstr contains `needle`."""
        needle_b = needle.encode("utf-8")
        base = self.dynstr["offset"]
        limit = base + self.dynstr["size"]
        pos = base
        while pos < limit:
            idx = self.data.find(b"\x00", pos, limit)
            if idx < 0:
                break
            s = bytes(self.data[pos:idx])
            if s.startswith(b"_Z") and needle_b in s:
                return True
            pos = idx + 1
        return False

    def write(self, path):
        with open(path, "wb") as f:
            f.write(self.data)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("file")
    ap.add_argument("--replace", action="append", default=[], metavar="OLD=NEW",
                    help="replace a string in .dynstr (may be given several times)")
    ap.add_argument("--list", action="store_true",
                    help="only print DT_NEEDED / DT_SONAME")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    with open(args.file, "rb") as f:
        elf = Elf(f.read())

    if args.list:
        print("SONAME:", elf.strtab(elf.soname()) if elf.soname() is not None else "(none)")
        for off in elf.needed():
            print("NEEDED:", elf.strtab(off))
        return 0

    if not args.replace:
        ap.error("nothing to do: pass --replace OLD=NEW or --list")

    total = 0
    for spec in args.replace:
        if "=" not in spec:
            ap.error("--replace expects OLD=NEW, got %r" % spec)
        old, new = spec.split("=", 1)
        n = elf.patch_string(old, new)
        total += n
        print("%s: %r -> %r : %d occurrence(s)" % (args.file, old, new, n))

    if total == 0:
        print("%s: nothing changed" % args.file)
        return 0

    if not args.dry_run:
        elf.write(args.file)
        print("%s: patched (%d bytes)" % (args.file, len(elf.data)))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except ElfError as e:
        print("error: %s" % e, file=sys.stderr)
        sys.exit(1)
