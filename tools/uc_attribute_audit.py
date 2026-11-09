"""Compare UC feedback records in local PC/Xbox VPAK banks, including inheritance."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

WAVE_KEYS = (0xBA1297DA, 0x0C149044, 0x8FD11D27, 0x35A8EADA,
             0x96A192E8, 0x7125C7EC, 0xAEF4FB7D, 0x1C2333F7, 0x3C714126, 0x751D40E5)
CLASSES = (0xFB111FEF, 0xD93FC53A)


def read_bank(path, endian):
    raw = path.read_bytes()
    if raw[:4] != b"VPAK":
        raise ValueError("Expected an uncompressed VPAK bank")
    _, blob_size, vlt_size, blob_at, vlt_at = struct.unpack_from("<5I", raw, 16)
    data = [raw[vlt_at:vlt_at+vlt_size], raw[blob_at:blob_at+blob_size]]
    vlt = data[0]
    chunks = {}
    at = 0
    while at < len(vlt):
        name = vlt[at:at+4].decode("ascii")
        if endian == "<":
            name = name[::-1]
        size = struct.unpack_from(endian+"I", vlt, at+4)[0]
        if size < 8 or at+size > len(vlt):
            raise ValueError("Invalid VLT chunk size")
        chunks[name] = (at, size)
        at += size
    pointers = {}
    at, size = chunks["PtrN"]
    target = None
    for pos in range(at+8, at+size, 12):
        fix, kind, index, offset = struct.unpack_from(endian+"IHHI", vlt, pos)
        if kind == 0:
            break
        if kind == 2:
            target = index
        elif kind == 3:
            pointers[target, fix] = (index, offset)
        elif kind == 1:
            pointers[target, fix] = None
        elif kind not in (4, 5):
            raise ValueError("Unknown relocation kind")
    at, _ = chunks["ExpN"]
    count = struct.unpack_from(endian+"I", vlt, at+8)[0]
    collections = {}
    for i in range(count):
        _, kind, size, offset = struct.unpack_from(endian+"4I", vlt, at+12+i*16)
        if kind != 0x8E112EB7:
            continue
        key, cls, parent, _, _, entries, counts, _ = struct.unpack_from(endian+"8I", vlt, offset)
        if cls not in CLASSES:
            continue
        type_count = struct.unpack_from(endian+"H", vlt, offset+24)[0]
        start = offset+32+type_count*4
        if start+entries*12 > offset+size:
            raise ValueError("Collection entries exceed export size")
        waves = {}
        for pos in range(start, start+entries*12, 12):
            attr = struct.unpack_from(endian+"I", vlt, pos)[0]
            if attr not in WAVE_KEYS:
                continue
            pointer = pointers.get((0, pos+4))
            records = []
            if pointer:
                dep, loc = pointer
                _, n, stride, padding = struct.unpack_from(endian+"4H", data[dep], loc)
                if stride != 32 or n > 4:
                    raise ValueError("Unexpected feedback wave array")
                loc += 8+(8 if padding & 0x8000 else 0)
                records = [struct.unpack_from(endian+"8f", data[dep], loc+j*32) for j in range(n)]
            waves[attr] = records
        # Different classes legitimately share a collection key (e.g. default).
        collections[cls, key] = (parent, waves)

    def resolve(cls, key, attr, visited):
        if key in visited:
            raise ValueError("Collection inheritance cycle")
        if (cls, key) not in collections:
            return []
        parent, own = collections[cls, key]
        if attr in own:
            return own[attr]
        return resolve(cls, parent, attr, visited | {key}) if parent else []

    result = {}
    for cls, key in collections:
        result[f"{cls:08X}/{key:08X}"] = {
            f"{attr:08X}": resolve(cls, key, attr, set()) for attr in WAVE_KEYS}
    return result


def compare(pc, xbox):
    a, b = read_bank(pc, "<"), read_bank(xbox, ">")
    shared = a.keys() & b.keys()
    differences = [key for key in sorted(shared) if a[key] != b[key]]
    return {"pc_sha256": hashlib.sha256(pc.read_bytes()).hexdigest(),
            "xbox_sha256": hashlib.sha256(xbox.read_bytes()).hexdigest(),
            "shared_collections": len(shared), "different_collections": differences,
            "pc_only": sorted(a.keys()-b.keys()), "xbox_only": sorted(b.keys()-a.keys()),
            "compared_records": sum(len(records) for coll in a.values() for records in coll.values())}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pc", type=Path)
    parser.add_argument("xbox", type=Path)
    args = parser.parse_args()
    result = compare(args.pc, args.xbox)
    print(json.dumps(result, indent=2))
    raise SystemExit(bool(result["different_collections"] or result["pc_only"] or result["xbox_only"]))
