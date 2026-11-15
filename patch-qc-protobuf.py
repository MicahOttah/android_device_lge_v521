#!/usr/bin/env python3
"""
Install the stock (Qualcomm CAF 3.5) protobuf runtime under a second name and
re-point the Qualcomm blobs that need it at that copy.

Why
---
The Connectivity Engine / radio blobs that LG shipped in the factory ROM
(libcne.so, libcneapiclient.so, libwqe.so, libsettings.so and bin/cnd) were
compiled against the CAF "3.5" protobuf runtime that LG put in
/system/lib/libprotobuf-cpp-{lite,full}.so.

LineageOS 15.1 ships the AOSP protobuf 3.0.0-beta-3 runtime in the very same
place.  It is a *newer* API but it does not contain the symbols the CAF blobs
import, so every one of them dies at load time:

    F libc : CANNOT LINK EXECUTABLE "/system/bin/netmgrd": cannot locate
      symbol "_ZN6google8protobuf2io16CodedInputStream20ReadVarint32FallbackEPj"
      referenced by "/system/vendor/lib/libcneapiclient.so"
    E RILD : dlopen failed: ... cannot locate symbol
      "_ZN6google8protobuf8internal21StringTypeHandlerBase3NewEv"
      referenced by "/system/vendor/lib/libsettings.so"

The two runtimes cannot simply be swapped (libhwui.so, libsurfaceflinger.so,
libtextclassifier.so, libmediadrm.so, libkeystore_binder.so,
bluetooth.default.so, perfprofd, libwebrtc_audio_preprocessing.so and
libclearkeycasplugin.so need the AOSP one), so both have to coexist - exactly
like they did in the stock ROM.

Because /system/lib is searched *before* /vendor/lib
(namespace.default.search.paths = /system/${LIB}:/vendor/${LIB}), the stock
runtime is installed as

    /system/vendor/lib/libqc-protobuf-lite.so
    /system/vendor/lib/libqc-protobuf-full.so

with its DT_SONAME rewritten to match, and the CAF blobs get their
DT_NEEDED rewritten from libprotobuf-cpp-{lite,full}.so to the new names.
Both names are one byte shorter than the originals, so the string table never
has to grow and the patched files keep their original layout.

This script is idempotent: it rewrites the vendor blobs from their pristine
state on every run, so it can be called before every build.

Usage:
    patch-qc-protobuf.py            install / re-point the blobs
    patch-qc-protobuf.py --check    only report the state of an installed tree
    patch-qc-protobuf.py --quiet    no output unless something is wrong
"""

import argparse
import importlib.util
import os
import shutil
import sys

sys.dont_write_bytecode = True  # do not litter the tree with __pycache__

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
VENDOR = os.path.join(REPO, "vendor", "lge", "v521")
PROP = os.path.join(VENDOR, "proprietary")
PREBUILT = os.path.join(HERE, "prebuilt", "qc-protobuf")

# pristine CAF runtime  ->  installed name
RUNTIMES = [
    ("libprotobuf-cpp-lite.so", "libqc-protobuf-lite.so",
     "libprotobuf-cpp-lite.so", "libqc-protobuf-lite.so"),
    ("libprotobuf-cpp-full.so", "libqc-protobuf-full.so",
     "libprotobuf-cpp-full.so", "libqc-protobuf-full.so"),
]

# blobs that import the CAF runtime, and which flavour they need
CONSUMERS = [
    ("vendor/lib/libcne.so", "lite"),
    ("vendor/lib/libcneapiclient.so", "lite"),
    ("vendor/lib/libwqe.so", "lite"),
    ("vendor/lib/libsettings.so", "full"),
    ("bin/cnd", "lite"),
]

# deliberately NOT patched: system/vendor/lib/mediadrm/libwvdrmengine.so also
# imports the CAF runtime, but mediadrmserver loads the AOSP libmediadrm.so in
# the same process, so re-pointing it would mix two protobuf runtimes in one
# address space.  Widevine is useless on this device anyway (no CDM), so the
# plugin is left broken on purpose - see notes/protobuf-runtime.md.

OLD = {"lite": "libprotobuf-cpp-lite.so", "full": "libprotobuf-cpp-full.so"}
NEW = {"lite": "libqc-protobuf-lite.so", "full": "libqc-protobuf-full.so"}

# libwqe.so is the only CAF blob that links AOSP's libandroid.so.  libandroid.so
# drags in libandroid_runtime.so -> libhwui.so / libmedia.so -> libmediadrm.so
# -> AOSP's protobuf runtime.  In a process that already has CAF's protobuf
# runtime in its global scope (cnd) the AOSP libraries bind their protobuf
# symbols to the CAF runtime and abort with a protobuf version mismatch.
#
# libcnesock.so (device/lge/v521/vendor/libcnesock) provides the only two
# symbols that libwqe.so imports from libandroid.so.  The replacement name
# has to be exactly as long as "libandroid.so", because the DT_NEEDED string
# is patched in place inside .dynstr.
#
# See notes/protobuf-runtime.md.
CUT_LIBANDROID = {
    "vendor/lib/libwqe.so": ("libandroid.so", "libcnesock.so"),
}

# libril-qc-qmi-1.so is the CAF radio implementation that rild dlopen()s
# (rild.libpath).  It links AOSP's libmedia.so, but does not use a single
# symbol from it: of its 271 undefined symbols none is exported by
# libmedia.so.  The edge exists only because the CAF build linked libmedia.so
# for the audio/metrics helpers it inherited from the AOSP tree.
#
# It has to be cut for the same reason as libwqe.so -> libandroid.so:
# libmedia.so -> libmediadrm.so -> libprotobuf-cpp-lite.so (AOSP) is the only
# remaining path by which the *AOSP* protobuf runtime enters the rild
# process, where libsettings.so already puts the *CAF* runtime into the global
# scope.  The linker resolves a symbol in the global group first, so
# libmediadrm.so's google::protobuf::internal::VerifyVersion() call would bind
# to the CAF runtime and abort with a protobuf version mismatch:
#
#   F libc : CANNOT LINK EXECUTABLE "/system/vendor/bin/hw/rild": ...
#   A abort: ... VerifyVersion(int, int, char const*)
#
# The replacement is libasys.so, a shim that pulls in AOSP's libaudioclient.so
# (which owns android::AudioSystem in Android 8.1) but no protobuf runtime.
# The name has to be no longer than "libmedia.so" (11 characters), because
# the DT_NEEDED string is patched in place inside libril-qc-qmi-1.so's
# .dynstr.
#
# See notes/protobuf-runtime.md.
CUT_LIBMEDIA = {
    "vendor/lib/libril-qc-qmi-1.so": ("libmedia.so", "libasys.so"),
}

# The protobuf C++ namespace inside the CAF runtime.
#
# Renaming it would make the two runtimes impossible to confuse: the CAF
# blobs would look for "googlE::protobuf" while the AOSP libraries keep
# looking for "google::protobuf".  Both names have the same length, so the
# string table keeps its size and no symbol offset moves.
#
# IT DOES NOT WORK, and it is therefore NOT applied:
#
#   CANNOT LINK EXECUTABLE "/system/bin/cnd": cannot locate symbol
#   "_ZN6googlE8protobuf8internal13empty_string_E" referenced by
#   "/system/vendor/lib/libcne.so"
#
# soinfo::gnu_lookup() (bionic/linker/linker_soinfo.cpp) computes
#   hash = symbol_name.gnu_hash()
# and starts the chain walk at gnu_bucket_[hash % gnu_nbucket_], comparing
# the names afterwards.  The renamed symbol hashes into a *different*
# bucket, so the linker never walks to it.  Making this work would mean
# rebuilding .gnu.hash and reordering .dynsym / .gnu.version / every
# relocation that indexes the symbol table.
#
# The real fix is to keep the AOSP protobuf runtime out of the CAF
# processes altogether - see notes/protobuf-runtime.md.
PB_NS_OLD = "6google8protobuf"
PB_NS_NEW = "6googlE8protobuf"

# Set to True to rename the protobuf namespace anyway (see above: it breaks
# the GNU hash table, so this is only useful for symbol-level experiments).
APPLY_NAMESPACE_RENAME = False


def load_elf_module():
    """Import the ELF helper that ships next to this script."""
    path = os.path.join(HERE, "patch-elf-dynstr.py")
    spec = importlib.util.spec_from_file_location("patch_elf_dynstr", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


ELF = load_elf_module()


def needed_of(path):
    with open(path, "rb") as f:
        elf = ELF.Elf(f.read())
    return [elf.strtab(o) for o in elf.needed()]


def soname_of(path):
    with open(path, "rb") as f:
        elf = ELF.Elf(f.read())
    off = elf.soname()
    return elf.strtab(off) if off is not None else None


def patch_file(path, replacements):
    """Apply {old: new} string replacements to an ELF's .dynstr in place."""
    before = os.path.getsize(path)
    with open(path, "rb") as f:
        elf = ELF.Elf(f.read())
    changed = 0
    for old, new in replacements.items():
        n = elf.patch_string(old, new)
        if n:
            changed += n
    if changed:
        elf.write(path)
    after = os.path.getsize(path)
    if after != before:
        raise RuntimeError("%s: size changed during patching (%d -> %d)" %
                           (path, before, after))
    return changed


def rename_namespace(path):
    """Rename the protobuf namespace of an ELF in .dynstr.

    Returns the number of symbol names that were changed.  Idempotent: a file
    that has already been renamed has nothing left to rename.
    """
    before = os.path.getsize(path)
    with open(path, "rb") as f:
        elf = ELF.Elf(f.read())
    n = elf.rename_substring(PB_NS_OLD, PB_NS_NEW)
    if n:
        elf.write(path)
    after = os.path.getsize(path)
    if after != before:
        raise RuntimeError("%s: size changed during renaming (%d -> %d)" %
                           (path, before, after))
    return n


def namespace_is_renamed(path):
    """True if the ELF no longer exports the original protobuf namespace."""
    with open(path, "rb") as f:
        elf = ELF.Elf(f.read())
    return not elf.has_substring(PB_NS_OLD)


def install_runtimes(verbose):
    """Copy the pristine CAF runtime in and fix its DT_SONAME."""
    ok = True
    for src_name, dst_name, old_so, new_so in RUNTIMES:
        src = os.path.join(PREBUILT, src_name)
        dst = os.path.join(PROP, "vendor/lib", dst_name)
        if not os.path.exists(src):
            print("patch-qc-protobuf: missing %s - run 'git submodule' or "
                  "restore the prebuilt blob" % src, file=sys.stderr)
            ok = False
            continue
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(src, dst)
        os.chmod(dst, 0o644)
        n = patch_file(dst, {old_so: new_so})
        r = rename_namespace(dst) if APPLY_NAMESPACE_RENAME else 0
        if verbose:
            print("  runtime %-26s <- %s (soname %d occurrence(s) patched, "
                  "%d symbol(s) renamed)" % (dst_name, src_name, n, r))
    return ok


def repoint_consumers(verbose):
    """Re-point the CAF blobs at the renamed runtime and rename their
    protobuf imports so that they can never bind to the AOSP runtime."""
    ok = True
    for rel, flavour in CONSUMERS:
        path = os.path.join(PROP, rel)
        if not os.path.exists(path):
            print("patch-qc-protobuf: missing blob %s" % path, file=sys.stderr)
            ok = False
            continue
        old, new = OLD[flavour], NEW[flavour]
        needed = needed_of(path)
        if new not in needed:
            if old not in needed:
                print("patch-qc-protobuf: %s does not link %s (needed: %s)" %
                      (rel, old, ", ".join(needed)), file=sys.stderr)
                ok = False
                continue
            patch_file(path, {old: new})
        r = rename_namespace(path) if APPLY_NAMESPACE_RENAME else 0
        if verbose:
            print("  %-42s -> %-24s (%d symbol(s) renamed)" %
                  (rel, new, r))
    return ok


def cut_edges(cuts, verbose):
    """Re-point a blob at a different library so that an unwanted edge to
    AOSP's library set disappears.

    Cutting the edge keeps libandroid_runtime.so / libhwui.so / libmedia.so
    / libmediadrm.so (and with them AOSP's protobuf runtime) out of the CAF
    processes.
    """
    ok = True
    for rel, (old, new) in cuts.items():
        path = os.path.join(PROP, rel)
        if not os.path.exists(path):
            print("patch-qc-protobuf: missing blob %s" % path, file=sys.stderr)
            ok = False
            continue
        before = os.path.getsize(path)
        with open(path, "rb") as f:
            elf = ELF.Elf(f.read())
        needed = [elf.strtab(o) for o in elf.needed()]
        if new in needed and old not in needed:
            if verbose:
                print("  %-42s already points at %s" % (rel, new))
            continue
        if old not in needed:
            print("patch-qc-protobuf: %s does not link %s" % (rel, old),
                  file=sys.stderr)
            ok = False
            continue
        elf.replace_needed(old, new)
        elf.write(path)
        after = os.path.getsize(path)
        if after != before:
            raise RuntimeError("%s: size changed during patching (%d -> %d)" %
                               (path, before, after))
        if verbose:
            print("  %-42s %s -> %s" % (rel, old, new))
    return ok


def check(system_dir):
    """Verify an installed system tree."""
    problems = 0
    for _src, dst_name, _old, new_so in RUNTIMES:
        p = os.path.join(system_dir, "system/vendor/lib", dst_name)
        if not os.path.exists(p):
            print("  MISSING  %s" % p)
            problems += 1
            continue
        so = soname_of(p)
        status = "ok" if so == new_so else "BAD SONAME %s" % so
        print("  %-34s soname=%-26s %s" % (dst_name, so, status))
        if so != new_so:
            problems += 1
        if APPLY_NAMESPACE_RENAME:
            renamed = namespace_is_renamed(p)
            print("  %-34s protobuf namespace renamed=%s" %
                  (dst_name, "yes" if renamed else "NO"))
            if not renamed:
                problems += 1
    for rel, flavour in CONSUMERS:
        p = os.path.join(system_dir, "system", rel)
        if not os.path.exists(p):
            print("  MISSING  %s" % p)
            problems += 1
            continue
        needed = needed_of(p)
        good = NEW[flavour] in needed and OLD[flavour] not in needed
        print("  %-42s %-26s %s" %
              (rel, ",".join(n for n in needed if "protobuf" in n),
               "ok" if good else "BAD"))
        if not good:
            problems += 1
        if APPLY_NAMESPACE_RENAME:
            renamed = namespace_is_renamed(p)
            if not renamed:
                print("  %-42s protobuf namespace NOT renamed" % rel)
                problems += 1
    for rel, (old, new) in CUT_LIBANDROID.items():
        p = os.path.join(system_dir, "system", rel)
        if not os.path.exists(p):
            print("  MISSING  %s" % p)
            problems += 1
            continue
        needed = needed_of(p)
        good = new in needed and old not in needed
        print("  %-42s %-26s %s" %
              (rel, ",".join(n for n in needed if "android" in n),
               "ok" if good else "BAD"))
        if not good:
            problems += 1
    for rel, (old, new) in CUT_LIBMEDIA.items():
        p = os.path.join(system_dir, "system", rel)
        if not os.path.exists(p):
            print("  MISSING  %s" % p)
            problems += 1
            continue
        needed = needed_of(p)
        good = new in needed and old not in needed
        print("  %-42s %-26s %s" %
              (rel, ",".join(n for n in needed if "media" in n),
               "ok" if good else "BAD"))
        if not good:
            problems += 1
    print("check: %d problem(s)" % problems)
    return 1 if problems else 0


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", metavar="SYSTEM_DIR",
                    help="only verify an already installed system tree")
    ap.add_argument("--quiet", action="store_true",
                    help="no output unless something is wrong")
    args = ap.parse_args()

    if args.check:
        return check(args.check)

    if not args.quiet:
        print("patch-qc-protobuf: installing CAF protobuf runtime")
    ok = install_runtimes(not args.quiet)
    if ok and not args.quiet:
        print("patch-qc-protobuf: re-pointing CAF blobs")
    ok = repoint_consumers(not args.quiet) and ok
    if ok and not args.quiet:
        print("patch-qc-protobuf: cutting the libwqe.so -> libandroid.so edge")
    ok = cut_edges(CUT_LIBANDROID, not args.quiet) and ok
    if ok and not args.quiet:
        print("patch-qc-protobuf: cutting the libril-qc-qmi-1.so -> libmedia.so edge")
    ok = cut_edges(CUT_LIBMEDIA, not args.quiet) and ok
    if not ok:
        print("patch-qc-protobuf: FAILED (build will fail the same way)",
              file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
