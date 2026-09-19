#!/usr/bin/env python3
"""Patch RT_MANIFEST requestedExecutionLevel requireAdministrator -> asInvoker."""

from __future__ import print_function

import argparse
import struct
import sys

RT_MANIFEST = 24
# Same-length replace. Pad *after* the closing quote so the attribute value is
# exactly asInvoker (trailing spaces inside the quotes are not a valid level).
OLD = b'level="requireAdministrator"'
NEW = b'level="asInvoker"' + (b" " * 11)


def die(msg):
    print("error: %s" % msg, file=sys.stderr)
    sys.exit(1)


def u16(data, off):
    if off < 0 or off + 2 > len(data):
        die("truncated PE (u16 at %d)" % off)
    return struct.unpack_from("<H", data, off)[0]


def u32(data, off):
    if off < 0 or off + 4 > len(data):
        die("truncated PE (u32 at %d)" % off)
    return struct.unpack_from("<I", data, off)[0]


def rva_to_off(rva, sections, file_len, what="RVA"):
    for va, vsize, rawptr, rawsize in sections:
        span = vsize if vsize else rawsize
        if va <= rva < va + span:
            off = rawptr + (rva - va)
            if off < 0 or off >= file_len:
                die("%s 0x%x maps outside file" % (what, rva))
            return off
    die("%s 0x%x not in any section" % (what, rva))


def parse_pe(data):
    if data[:2] != b"MZ":
        die("not an MZ image")
    e_lfanew = u32(data, 0x3C)
    if data[e_lfanew : e_lfanew + 4] != b"PE\x00\x00":
        die("not a PE image")
    coff = e_lfanew + 4
    num_sections = u16(data, coff + 2)
    size_opt = u16(data, coff + 16)
    opt = coff + 20
    magic = u16(data, opt)
    if magic == 0x10B:
        num_rva = u32(data, opt + 92)
        dd = opt + 96
    elif magic == 0x20B:
        num_rva = u32(data, opt + 108)
        dd = opt + 112
    else:
        die("unknown optional header magic 0x%x" % magic)
    if size_opt < (dd - opt) + 24:
        die("optional header too small for resource data directory")
    if num_rva < 3:
        die("PE has no resource data directory")
    res_rva = u32(data, dd + 16)
    res_size = u32(data, dd + 20)
    if res_rva == 0 or res_size == 0:
        die("empty resource directory")

    sec_off = opt + size_opt
    sections = []
    for i in range(num_sections):
        s = sec_off + i * 40
        if s + 40 > len(data):
            die("truncated section table")
        _name, vsize, va, rawsize, rawptr = struct.unpack_from("<8sIIII", data, s)
        sections.append((va, vsize, rawptr, rawsize))
    return res_rva, res_size, sections, magic


def iter_dir_entries(data, res_off, dir_rel, res_end):
    off = res_off + dir_rel
    if off + 16 > res_end or off + 16 > len(data):
        die("truncated resource directory")
    named = u16(data, off + 12)
    ids = u16(data, off + 14)
    total = named + ids
    ent = off + 16
    if ent + total * 8 > res_end or ent + total * 8 > len(data):
        die("truncated resource directory entries")
    for i in range(total):
        name = u32(data, ent + i * 8)
        offset = u32(data, ent + i * 8 + 4)
        yield (name & 0x7FFFFFFF, bool(name & 0x80000000),
               offset & 0x7FFFFFFF, bool(offset & 0x80000000))


def walk_manifests(data, res_rva, sections):
    res_off = rva_to_off(res_rva, sections, len(data), "resource directory")
    # Walk only as far as the mapped resource tree; data blobs use their own RVAs.
    res_end = len(data)

    found = []

    def walk(dir_rel, depth, type_id):
        if depth > 8:
            die("resource directory nesting too deep")
        for ident, is_name, sub, is_dir in iter_dir_entries(
                data, res_off, dir_rel, res_end):
            cur_type = type_id
            if depth == 0:
                if is_name or ident != RT_MANIFEST:
                    continue
                cur_type = ident
            if is_dir:
                walk(sub, depth + 1, cur_type)
                continue
            entry = res_off + sub
            if entry + 16 > len(data):
                die("truncated resource data entry")
            data_rva = u32(data, entry)
            size = u32(data, entry + 4)
            file_off = rva_to_off(data_rva, sections, len(data), "manifest data")
            if size < 0 or file_off + size > len(data):
                die("manifest data exceeds file")
            found.append((file_off, size))

    walk(0, 0, None)
    return found


def snippet_around(buf, pos, length, ctx=48):
    start = max(0, pos - ctx)
    end = min(len(buf), pos + length + ctx)
    chunk = buf[start:end]
    return chunk.decode("ascii", errors="replace")


def patch_bytes(data):
    if len(OLD) != len(NEW):
        die("length mismatch: %d vs %d" % (len(OLD), len(NEW)))
    if len(OLD) != 28:
        die("length mismatch: level=\"requireAdministrator\" is 28 chars")

    res_rva, _res_size, sections, magic = parse_pe(data)
    blobs = walk_manifests(data, res_rva, sections)
    if not blobs:
        die("RT_MANIFEST (24) not found")

    hits = []
    out = bytearray(data)
    for file_off, size in blobs:
        blob = bytes(out[file_off : file_off + size])
        if OLD not in blob:
            continue
        n = blob.count(OLD)
        patched = blob.replace(OLD, NEW)
        if len(patched) != len(blob):
            die("length mismatch after replace")
        out[file_off : file_off + size] = patched
        pos = blob.find(OLD)
        hits.append((file_off + pos, blob, patched, n))

    if not hits:
        die("ASCII substring level=\"requireAdministrator\" not found in RT_MANIFEST")

    file_pos, old_blob, new_blob, n = hits[0]
    old_pos = old_blob.find(OLD)
    new_pos = new_blob.find(NEW)
    print("PE magic=0x%x RT_MANIFEST replacements=%d file_off=0x%x" %
          (magic, sum(h[3] for h in hits), file_pos))
    print("old: %s" % snippet_around(old_blob, old_pos, len(OLD)))
    print("new: %s" % snippet_around(new_blob, new_pos, len(NEW)))
    return bytes(out)


def patch_runas_verb(data):
    """Replace ShellExecute verb 'runas' with 'open\\0' (same 5 bytes) so Mch
    is not force-elevated when CameraCutCore launches it."""
    out = bytearray(data)
    old = b"runas"
    new = b"open\x00"
    if len(old) != len(new):
        die("runas/open length mismatch")
    count = 0
    start = 0
    while True:
        pos = out.find(old, start)
        if pos < 0:
            break
        # Prefer isolated verb: not part of a longer ASCII token
        before = out[pos - 1] if pos > 0 else 0
        after = out[pos + len(old)] if pos + len(old) < len(out) else 0
        if before >= 0x41 and before <= 0x7A:
            start = pos + 1
            continue
        if after >= 0x41 and after <= 0x7A:
            start = pos + 1
            continue
        out[pos : pos + len(old)] = new
        count += 1
        start = pos + len(old)
    print("runas->open replacements=%d" % count)
    return bytes(out)


def main(argv=None):
    p = argparse.ArgumentParser(
        description="Replace RT_MANIFEST requireAdministrator with asInvoker (same length).")
    p.add_argument("infile", help="PE32/PE32+ image")
    p.add_argument("--in-place", action="store_true",
                   help="overwrite INFILE")
    p.add_argument("--output", metavar="OUT",
                   help="write patched copy to OUT")
    p.add_argument("--also-patch-runas", action="store_true",
                   help="also replace ShellExecute verb runas with open (for CameraCutCore)")
    args = p.parse_args(argv)

    try:
        with open(args.infile, "rb") as f:
            data = f.read()
    except OSError as e:
        die("cannot read %s: %s" % (args.infile, e))

    if OLD in data:
        patched = patch_bytes(data)
    elif b'level="asInvoker"' in data:
        print("manifest already asInvoker; skipping level patch")
        patched = data
    else:
        die("no requireAdministrator or asInvoker level= in file")

    if args.also_patch_runas:
        patched = patch_runas_verb(patched)

    dest = None
    if args.output:
        dest = args.output
    elif args.in_place:
        dest = args.infile
    else:
        print("dry-run: not writing (pass --in-place or --output)")
        return 0

    try:
        with open(dest, "wb") as f:
            f.write(patched)
    except OSError as e:
        die("cannot write %s: %s" % (dest, e))
    print("wrote %s (%d bytes)" % (dest, len(patched)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
