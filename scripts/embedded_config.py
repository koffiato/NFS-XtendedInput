"""Read build configuration resources without loading or executing an ASI."""
import hashlib
import json
from pathlib import Path
import struct


def resources(binary):
    """Read the integer RCDATA resources in a PE32 DLL."""
    try:
        pe = struct.unpack_from("<I", binary, 0x3C)[0]
        count, optional_size = struct.unpack_from("<H12xH", binary, pe + 6)
        optional = pe + 24
        if optional_size < 120 or struct.unpack_from("<H", binary, optional)[0] != 0x10B:
            raise ValueError("Expected a PE32 resource directory")
        resource_rva, resource_size = struct.unpack_from("<II", binary, optional + 112)
        sections = []
        for index in range(count):
            header = optional + optional_size + index * 40
            _, rva, size, offset = struct.unpack_from("<IIII", binary, header + 8)
            sections.append((rva, size, offset))

        def offset_of(rva, size):
            for start, length, offset in sections:
                if start <= rva and rva + size <= start + length:
                    offset += rva - start
                    if offset + size <= len(binary):
                        return offset
            raise ValueError("Resource points outside the PE file")

        if not resource_rva or not resource_size:
            raise ValueError("Missing embedded configuration; rebuild the ASI")
        base = offset_of(resource_rva, resource_size)

        def directory(relative):
            if relative < 0 or relative + 16 > resource_size:
                raise ValueError("Invalid resource directory")
            named, numbered = struct.unpack_from("<HH", binary, base + relative + 12)
            if relative + 16 + (named + numbered) * 8 > resource_size:
                raise ValueError("Truncated resource directory")
            entries = {}
            for index in range(named + numbered):
                name, value = struct.unpack_from("<II", binary, base + relative + 16 + index * 8)
                if not name & 0x80000000:
                    if name in entries:
                        raise ValueError("Duplicate resource identifier")
                    entries[name] = value
            return entries

        def child(value):
            if not value & 0x80000000:
                raise ValueError("Expected a resource directory")
            return directory(value & 0x7FFFFFFF)

        types = directory(0)
        if 10 not in types:
            raise ValueError("Missing configuration RCDATA resources")
        result = {}
        for identifier, entry in child(types[10]).items():
            if identifier not in {101, 102, 103, 104}:
                continue
            languages = child(entry)
            if len(languages) != 1:
                raise ValueError("Expected one language for configuration resources")
            data_entry = next(iter(languages.values()))
            if data_entry & 0x80000000 or data_entry + 16 > resource_size:
                raise ValueError("Invalid resource data entry")
            rva, size = struct.unpack_from("<II", binary, base + data_entry)
            offset = offset_of(rva, size)
            result[identifier] = binary[offset:offset + size]
        return result
    except (struct.error, IndexError) as error:
        raise ValueError("Truncated PE configuration resources") from error


def input_hash(data):
    # Inputs are text; CRLF and LF checkouts describe the same configuration.
    return hashlib.sha256(data.replace(b"\r", b"")).hexdigest()


def input_paths(root):
    paths = (root / "config/inputs.txt").read_text(encoding="utf-8").splitlines()
    paths = [name for name in paths if name]
    if len(paths) != len(set(paths)) or "config/inputs.txt" not in paths:
        raise ValueError("Invalid configuration input list")
    for name in paths:
        path = Path(name)
        if path.is_absolute() or ".." in path.parts or "\\" in name:
            raise ValueError(f"Expected a relative configuration input: {name}")
        if not (root / path).resolve().is_relative_to(root.resolve()):
            raise ValueError(f"Configuration input escapes the source root: {name}")
    return paths


def configuration(binary, game, root):
    embedded = resources(binary)
    expected = {101, 103, 104} | ({102} if not game.underground else set())
    if set(embedded) != expected:
        raise ValueError("Missing or unexpected configuration resources; rebuild the ASI")
    try:
        manifest = json.loads(embedded[104])
        if manifest["target"] != game.target:
            raise ValueError(f"ASI target differs from package target {game.target}")
        paths = input_paths(root)
        if set(manifest["inputs"]) != set(paths):
            raise ValueError("Configuration input list changed; rebuild the ASI")
        for name in paths:
            if manifest["inputs"][name] != input_hash((root / name).read_bytes()):
                raise ValueError(f"Stale configuration input {name}; rebuild the ASI")
    except (KeyError, TypeError, json.JSONDecodeError, UnicodeDecodeError) as error:
        raise ValueError("Invalid embedded config manifest; rebuild the ASI") from error
    except OSError as error:
        raise ValueError("Missing configuration source input; rebuild the ASI") from error
    files = {f"{game.plugin}.ini": embedded[101], "EventReference.txt": embedded[103]}
    if not game.underground:
        files[f"{game.plugin}.default.ini"] = embedded[102]
    return files
