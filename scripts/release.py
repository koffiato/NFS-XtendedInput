"""Build installable archives using the upstream XtendedInput release layout."""
import argparse
from dataclasses import dataclass
import hashlib
from pathlib import Path
import re
import struct
import urllib.request
import zipfile


@dataclass(frozen=True)
class Game:
    target: str
    name: str
    upstream_sha256: str
    feedback: str
    underground: bool = False
    texture: str = ""
    cursor: bool = False

    @property
    def archive(self):
        return f"Release-{self.name}-Pack.zip"

    @property
    def plugin(self):
        return "NFSU_XtendedInput" if self.underground else "NFS_XtendedInput"

    @property
    def upstream_url(self):
        repo, tag = ("NFSU-XtendedInput", "2.7") if self.underground else ("NFS-XtendedInput", "1.22")
        return f"https://github.com/xan1242/{repo}/releases/download/{tag}/{self.archive}"

    @property
    def resources(self):
        files = ["dinput8.dll"]
        if self.texture:
            files.append(self.texture)
        if self.cursor:
            files.append("scripts/nfs_cursor.cur")
        return files


GAMES = {
    g.target: g for g in (
        Game("UG", "UG", "c6858ff642209ad370a1aa3472337444080c841e8edc1aa9b19c95717a85d1fb",
             "Experimental US 1.4 rumble adapter; disabled in the supplied INI. Gameplay validation is pending.",
             underground=True, texture="Global/UG_ConsoleButtons.tpk"),
        Game("UG2", "UG2", "1b80e9fa339bd24b4190eaafa4fa3fcf8438dfecc88fad82e8e40a590b8f5bc7",
             "US 1.2 native feedback and tuned impulse triggers, validated in local gameplay. Executable integration and runtime checks pass.",
             underground=True),
        Game("MW", "MW", "a78fbf5a2ba672557b23d7633175f391563a264c064762b2b50df88fb1a57fd0",
             "PC 1.3 rumble adapter retained from the fork. The shared refactor needs in-game controller testing.",
             texture="GLOBAL/XtendedInputButtons.tpk", cursor=True),
        Game("CB", "Carbon", "86c019cd79a1625c4061d5c7b1bd42cca5350c2d2271f8257ec612e752804001",
             "Input support only. Rumble is not implemented.", texture="GLOBAL/XtendedInputButtons.tpk", cursor=True),
        Game("UC", "UC", "fd0b42b2688de16917d360897c7da9baf25a7426f318b6ad57e79bc6e780ccc4",
             "Input support only. Rumble is not implemented."),
        Game("PS", "ProStreet", "15ad3e2996cb340aefcbd5c3eb43b7d3524c954657f17dd292fe9435efb9b01a",
             "Experimental native PC rumble adapter, with live surface/vehicle data and temporary MW effect tuning. Xbox 360 curves and controller validation are pending.", texture="GLOBAL/XtendedInputButtons.tpk", cursor=True),
        Game("W", "World", "4556330af357bc1118043a9de9392db90d040db58bdb0eee331f458cadb473f7",
             "Experimental upstream input support. Rumble is not implemented."),
    )
}


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def validate_binary(data):
    """Reject a missing, truncated or wrong-architecture build before packaging."""
    if len(data) < 64 or data[:2] != b"MZ":
        raise ValueError("Expected an x86 PE DLL, not an empty or invalid build")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if pe + 26 > len(data) or data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("Invalid PE header")
    if (struct.unpack_from("<H", data, pe + 4)[0] != 0x14C
            or not struct.unpack_from("<H", data, pe + 22)[0] & 0x2000
            or struct.unpack_from("<H", data, pe + 24)[0] != 0x10B):
        raise ValueError("Release plugin must be a 32-bit x86 DLL")


def source_url(repository, commit):
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository):
        raise ValueError("Expected an owner/repository name")
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("Expected the full source commit SHA")
    return f"https://github.com/{repository}/blob/{commit}"


def obtain_upstream(game, cache):
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache / game.archive
    if not archive.exists():
        # Only mod resources are downloaded. No game executable is needed.
        with urllib.request.urlopen(game.upstream_url, timeout=90) as response:
            data = response.read(16 * 1024 * 1024 + 1)
        if len(data) > 16 * 1024 * 1024 or sha256(data) != game.upstream_sha256:
            raise ValueError(f"Upstream checksum mismatch: {game.archive}")
        archive.write_bytes(data)
    if sha256(archive.read_bytes()) != game.upstream_sha256:
        raise ValueError(f"Upstream checksum mismatch: {archive}")
    return archive


def package(root, game, upstream, output, repository, commit):
    source = source_url(repository, commit)
    if sha256(upstream.read_bytes()) != game.upstream_sha256:
        raise ValueError(f"Upstream checksum mismatch: {upstream}")
    plugin = f"scripts/{game.plugin}.asi"
    binary = (root / "build" / game.target / f"{game.plugin}.asi").read_bytes()
    validate_binary(binary)
    config = root / "data" / f"Config-{game.name}"
    # Read selected assets only; never extract or ship upstream's older ASI/configs.
    with zipfile.ZipFile(upstream) as original:
        files = {name: original.read(name) for name in game.resources}
    files[plugin] = binary
    files[f"scripts/{game.plugin}.ini"] = (config / f"{game.plugin}.ini").read_bytes()
    if not game.underground:
        files[f"scripts/{game.plugin}.default.ini"] = (config / f"{game.plugin}.default.ini").read_bytes()
    files["EventReference.txt"] = (config / "EventReference.txt").read_bytes()
    files["LICENSE"] = (root / "LICENSE").read_bytes()
    files["README.md"] = f"""# XtendedInput - {game.name}

Development build for Windows 10/11, 32-bit game executables.

**Feedback status:** {game.feedback}

## Install

Back up your current XtendedInput ASI, INIs and custom mappings. Extract this
archive into the matching game's directory; its folders are already arranged
for installation. Install only one XtendedInput ASI per game. Keep your existing
INIs if you want to preserve bindings, and merge new settings as needed.

The ASI loader and game-specific textures/cursor are included where upstream
ships them. The x86 Microsoft Visual C++ runtime is required.

## Configuration and source

Settings are in `scripts/{game.plugin}.ini`. See `EventReference.txt` for actions.
UG/UG2 bindings use the main INI; later games use per-profile user maps.

[Full instructions and support matrix]({source}/README.md).
Source commit: `{commit}`.

Loader, texture and cursor resources come from the
[official upstream package]({game.upstream_url}); the plugin and configurations
come from this source commit. No game files are included.
""".encode("utf-8")
    output.mkdir(parents=True, exist_ok=True)
    destination = output / game.archive
    # Fixed metadata and sorted paths make packaging identical for identical inputs.
    with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(name, date_time=(2024, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.create_system = 3
            info.external_attr = 0o100644 << 16
            archive.writestr(info, data)
    with zipfile.ZipFile(destination) as archive:
        if archive.testzip() is not None or archive.read(plugin) != binary:
            raise ValueError(f"Archive verification failed: {destination}")
    return destination


def checksums(output):
    expected = {game.archive for game in GAMES.values()}
    actual = {path.name for path in output.glob("*.zip")}
    if actual != expected:
        raise ValueError(f"Release ZIP set differs: missing={sorted(expected-actual)}, unexpected={sorted(actual-expected)}")
    manifest = output / "SHA256SUMS.txt"
    manifest.write_text("".join(f"{sha256((output/name).read_bytes())}  {name}\n" for name in sorted(expected)), encoding="ascii")
    return manifest


def release_notes(repository, commit):
    source = source_url(repository, commit)
    rows = "\n".join(f"| `{game.archive}` | {game.feedback} |" for game in GAMES.values())
    return f"""Unified XtendedInput development build for Windows 10/11.

**Seven input targets are included; rumble is not implemented for all seven games.**

| Download | Feedback status |
| --- | --- |
{rows}

Each ZIP follows the matching official release layout: extract it into that
game's directory. Back up existing ASIs, INIs and custom mappings first.
Loader and game-specific resources are included; the x86 Visual C++ runtime
is required. No Windows 7 builds or game files are included.

[Installation, settings and validation limits]({source}/README.md).
SHA-256 checksums are attached as `SHA256SUMS.txt`.
Source: `{commit}`.
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    pack = sub.add_parser("package")
    pack.add_argument("--game", required=True, choices=GAMES)
    pack.add_argument("--upstream-dir", type=Path, default=Path(".cache/upstream"))
    pack.add_argument("--output-dir", type=Path, default=Path("dist"))
    pack.add_argument("--repository", required=True)
    pack.add_argument("--commit", required=True)
    sums = sub.add_parser("checksums")
    sums.add_argument("--output-dir", type=Path, default=Path("dist"))
    notes = sub.add_parser("notes")
    notes.add_argument("--repository", required=True)
    notes.add_argument("--commit", required=True)
    notes.add_argument("--output-file", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "package":
        game = GAMES[args.game]
        source_url(args.repository, args.commit)
        print(package(Path(__file__).resolve().parents[1], game, obtain_upstream(game, args.upstream_dir),
                      args.output_dir, args.repository, args.commit))
    elif args.command == "checksums":
        print(checksums(args.output_dir))
    else:
        args.output_file.write_text(release_notes(args.repository, args.commit), encoding="utf-8")


if __name__ == "__main__":
    main()
