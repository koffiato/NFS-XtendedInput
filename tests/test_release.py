"""Exercise installable archive contents and refusal of invalid release inputs."""
from dataclasses import replace
import hashlib
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

SPEC = importlib.util.spec_from_file_location("release", Path(__file__).resolve().parents[1] / "scripts/release.py")
release = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = release
SPEC.loader.exec_module(release)
COMMIT = "0123456789abcdef0123456789abcdef01234567"


def pe_binary(machine=0x14C):
    data = bytearray(256)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 128)
    data[128:132] = b"PE\0\0"
    struct.pack_into("<H", data, 132, machine)
    struct.pack_into("<H", data, 150, 0x2000)
    struct.pack_into("<H", data, 152, 0x10B)
    return bytes(data)


class PackagingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "LICENSE").write_text("Fixture license")

    def inputs(self, game):
        config = self.root / "data" / f"Config-{game.name}"
        build = self.root / "build" / game.target
        config.mkdir(parents=True)
        build.mkdir(parents=True)
        (build / f"{game.plugin}.asi").write_bytes(pe_binary())
        (config / f"{game.plugin}.ini").write_bytes(b"[Rumble]\nEnabled = 0\n")
        (config / "EventReference.txt").write_bytes(b"current action reference")
        if not game.underground:
            (config / f"{game.plugin}.default.ini").write_bytes(b"current bindings")
        upstream = self.root / game.archive
        with zipfile.ZipFile(upstream, "w") as z:
            for name in game.resources:
                z.writestr(name, b"resource:" + name.encode())
            z.writestr(f"scripts/{game.plugin}.asi", b"old upstream plugin")
            z.writestr(f"scripts/{game.plugin}.ini", b"old upstream config")
            z.writestr("../not-an-asset", b"excluded")
        return replace(game, upstream_sha256=hashlib.sha256(upstream.read_bytes()).hexdigest()), upstream

    def test_all_seven_match_official_install_layout(self):
        for game in release.GAMES.values():
            with self.subTest(game=game.target):
                game, upstream = self.inputs(game)
                output = release.package(self.root, game, upstream, self.root / "dist", "owner/repo", COMMIT)
                with zipfile.ZipFile(output) as z:
                    expected = set(game.resources) | {"README.md", "LICENSE", "EventReference.txt",
                        f"scripts/{game.plugin}.asi", f"scripts/{game.plugin}.ini"}
                    if not game.underground:
                        expected.add(f"scripts/{game.plugin}.default.ini")
                    self.assertEqual(set(z.namelist()), expected)
                    self.assertEqual(z.read(f"scripts/{game.plugin}.asi"), pe_binary())
                    self.assertEqual(z.read(f"scripts/{game.plugin}.ini"), b"[Rumble]\nEnabled = 0\n")
                    self.assertIn(COMMIT.encode(), z.read("README.md"))
                    self.assertIn(game.feedback.encode(), z.read("README.md"))
                    for name in game.resources:
                        self.assertEqual(z.read(name), b"resource:" + name.encode())
                    self.assertIsNone(z.testzip())
                before = output.read_bytes()
                release.package(self.root, game, upstream, output.parent, "owner/repo", COMMIT)
                self.assertEqual(output.read_bytes(), before)
        sums = release.checksums(self.root / "dist").read_text()
        self.assertEqual(len(sums.splitlines()), 7)
        for line in sums.splitlines():
            digest, name = line.split("  ")
            self.assertEqual(digest, release.sha256((self.root / "dist" / name).read_bytes()))

    def test_tampered_upstream_rejected_before_packaging(self):
        game, upstream = self.inputs(release.GAMES["UG2"])
        upstream.write_bytes(upstream.read_bytes() + b"changed")
        with self.assertRaisesRegex(ValueError, "checksum mismatch"):
            release.package(self.root, game, upstream, self.root / "dist", "owner/repo", COMMIT)
        self.assertFalse((self.root / "dist").exists())

    def test_cached_download_is_also_verified(self):
        game, _ = self.inputs(release.GAMES["UG2"])
        with patch.object(release.urllib.request, "urlopen", side_effect=AssertionError("no network needed")):
            self.assertEqual(release.obtain_upstream(game, self.root), self.root / game.archive)
            with self.assertRaisesRegex(ValueError, "checksum mismatch"):
                release.obtain_upstream(release.GAMES["UG2"], self.root)

    def test_wrong_architecture_and_truncated_plugins_rejected(self):
        for data in (b"", b"MZ", pe_binary(0x8664)):
            with self.subTest(size=len(data)):
                with self.assertRaises(ValueError):
                    release.validate_binary(data)

    def test_incomplete_release_cannot_create_checksums(self):
        output = self.root / "dist"
        output.mkdir()
        (output / "Release-UG2-Pack.zip").write_bytes(b"partial release")
        with self.assertRaisesRegex(ValueError, "Release ZIP set differs"):
            release.checksums(output)

    def test_notes_include_limits_and_every_download(self):
        notes = release.release_notes("owner/repo", COMMIT)
        self.assertIn("rumble is not implemented for all seven games", notes)
        for game in release.GAMES.values():
            self.assertIn(game.archive, notes)
            self.assertIn(game.feedback, notes)
        with self.assertRaises(ValueError):
            release.release_notes("owner/repo", "main")


if __name__ == "__main__":
    unittest.main()
