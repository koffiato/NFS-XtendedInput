"""Regressions for stale builds, target identity and exact embedded INI bytes."""
from pathlib import Path
import json
import struct
import sys
import tempfile
import unittest

from config_helpers import REPOSITORY, copy_configuration_sources, config_resources, pe_binary

sys.path.insert(0, str(REPOSITORY / "scripts"))
from embedded_config import configuration, resources
from targets import GAMES


class EmbeddedConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        copy_configuration_sources(self.root)

    def test_each_target_exports_exact_built_text_and_comments(self):
        for game in GAMES.values():
            with self.subTest(game=game.target):
                built = config_resources(self.root, game)
                files = configuration(pe_binary(built), game, self.root)
                self.assertEqual(files[f"{game.plugin}.ini"], built[101])
                self.assertEqual(files["EventReference.txt"], built[103])
                self.assertEqual(len(files), 2 if game.underground else 3)
                if not game.underground:
                    self.assertEqual(files[f"{game.plugin}.default.ini"], built[102])

    def test_changed_settings_bindings_or_event_tables_require_rebuilding(self):
        game = GAMES["MW"]
        binary = pe_binary(config_resources(self.root, game))
        for name in ("config/settings.def", "config/bindings.csv", "src/input/Actions.h"):
            with self.subTest(source=name):
                path = self.root / name
                before = path.read_bytes()
                path.write_bytes(before + b"\nchanged\n")
                with self.assertRaisesRegex(ValueError, "Stale configuration input.*rebuild"):
                    configuration(binary, game, self.root)
                path.write_bytes(before)

    def test_packaging_does_not_need_generated_files_on_disk(self):
        game = GAMES["UG"]
        binary = pe_binary(config_resources(self.root, game))
        self.assertFalse((self.root / "build").exists())
        self.assertEqual(configuration(binary, game, self.root)[f"{game.plugin}.ini"], resources(binary)[101])

    def test_source_line_ending_changes_are_not_stale_builds(self):
        game = GAMES["CB"]
        binary = pe_binary(config_resources(self.root, game))
        path = self.root / "config/settings.def"
        path.write_bytes(path.read_bytes().replace(b"\r\n", b"\n").replace(b"\n", b"\r\n"))
        configuration(binary, game, self.root)

    def test_wrong_target_missing_manifest_and_invalid_json_are_rejected(self):
        game = GAMES["MW"]
        built = config_resources(self.root, game)
        with self.assertRaisesRegex(ValueError, "target differs"):
            configuration(pe_binary(built), GAMES["CB"], self.root)
        without_manifest = dict(built)
        del without_manifest[104]
        with self.assertRaisesRegex(ValueError, "Missing or unexpected"):
            configuration(pe_binary(without_manifest), game, self.root)
        built[104] = b"{invalid JSON"
        with self.assertRaisesRegex(ValueError, "Invalid embedded config manifest"):
            configuration(pe_binary(built), game, self.root)

    def test_changed_input_list_and_escaping_paths_are_rejected(self):
        game = GAMES["PS"]
        built = config_resources(self.root, game)
        manifest = json.loads(built[104])
        del manifest["inputs"]["config/settings.def"]
        built[104] = json.dumps(manifest).encode()
        with self.assertRaisesRegex(ValueError, "input list changed"):
            configuration(pe_binary(built), game, self.root)
        (self.root / "config/inputs.txt").write_text("config/inputs.txt\n../outside.txt\n")
        with self.assertRaisesRegex(ValueError, "relative configuration input"):
            configuration(pe_binary(built), game, self.root)

    def test_truncated_and_out_of_bounds_resources_are_rejected(self):
        built = pe_binary(config_resources(self.root, GAMES["MW"]))
        for data in (b"", built[:200], built[:-512]):
            with self.subTest(length=len(data)), self.assertRaises(ValueError):
                resources(data)
        invalid = bytearray(built)
        struct.pack_into("<I", invalid, 152 + 112, 0xFFFFFF00)
        with self.assertRaisesRegex(ValueError, "outside the PE"):
            resources(invalid)


if __name__ == "__main__":
    unittest.main()
