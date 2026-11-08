"""Small PE/resource fixtures for packaging tests; no compiler or game required."""
from pathlib import Path
import hashlib
import json
import shutil
import struct

REPOSITORY = Path(__file__).resolve().parents[1]


def copy_configuration_sources(root):
    names = (REPOSITORY / "config/inputs.txt").read_text().splitlines()
    for name in names:
        destination = root / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(REPOSITORY / name, destination)


def config_resources(root, game):
    paths = (root / "config/inputs.txt").read_text().splitlines()
    manifest = {"target": game.target, "inputs": {
        name: hashlib.sha256((root / name).read_bytes().replace(b"\r", b"")).hexdigest()
        for name in paths
    }}
    result = {
        101: b"; Fixture settings and comments\r\n[Deadzone]\r\nPercentLS = 0.08\r\n",
        103: b"Fixture event reference\r\n",
        104: json.dumps(manifest).encode(),
    }
    if not game.underground:
        result[102] = b"; Fixture binding comments\r\n[Events]\r\nGAMEACTION_GAS = XINPUT_GAMEPAD_RT\r\n"
    return result


def pe_binary(resources=None, machine=0x14C):
    """Construct a PE32 DLL header and genuine three-level resource directory."""
    resources = resources or {}
    count = len(resources)
    block = bytearray(40 + count * (8 + 24 + 16)) if count else bytearray()
    if count:
        struct.pack_into("<H", block, 14, 1)
        struct.pack_into("<II", block, 16, 10, 0x80000000 | 24)
        struct.pack_into("<H", block, 24 + 14, count)
    for index, (identifier, content) in enumerate(sorted(resources.items())):
        name_directory = 40 + count * 8 + index * 24
        data_entry = 40 + count * (8 + 24) + index * 16
        struct.pack_into("<II", block, 40 + index * 8, identifier, 0x80000000 | name_directory)
        struct.pack_into("<H", block, name_directory + 14, 1)
        struct.pack_into("<II", block, name_directory + 16, 1033, data_entry)
        while len(block) % 4:
            block.append(0)
        struct.pack_into("<II", block, data_entry, 0x1000 + len(block), len(content))
        block.extend(content)
    raw_size = (len(block) + 511) // 512 * 512
    data = bytearray(512 + raw_size)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 128)
    data[128:132] = b"PE\0\0"
    struct.pack_into("<HH", data, 132, machine, 1 if count else 0)
    struct.pack_into("<HH", data, 148, 224, 0x2000)
    struct.pack_into("<H", data, 152, 0x10B)
    if count:
        struct.pack_into("<II", data, 152 + 112, 0x1000, len(block))
        data[376:384] = b".rsrc\0\0\0"
        struct.pack_into("<IIII", data, 384, len(block), 0x1000, raw_size, 512)
        data[512:512 + len(block)] = block
    return bytes(data)
