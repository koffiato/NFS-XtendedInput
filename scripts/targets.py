"""Supported game targets and pinned upstream mod resources."""
from dataclasses import dataclass


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
             "US 1.4 native rumble adapter; enabled in the supplied INI and respects the in-game Vibration option. Locally tested; no impulse triggers.",
             underground=True, texture="Global/UG_ConsoleButtons.tpk"),
        Game("UG2", "UG2", "1b80e9fa339bd24b4190eaafa4fa3fcf8438dfecc88fad82e8e40a590b8f5bc7",
             "US 1.2 native feedback and tuned impulse triggers, validated in local gameplay. Executable integration and runtime checks pass.",
             underground=True),
        Game("MW", "MW", "a78fbf5a2ba672557b23d7633175f391563a264c064762b2b50df88fb1a57fd0",
             "PC 1.3 rumble adapter retained from the fork. The shared refactor needs in-game controller testing.",
             texture="GLOBAL/XtendedInputButtons.tpk", cursor=True),
        Game("CB", "Carbon", "86c019cd79a1625c4061d5c7b1bd42cca5350c2d2271f8257ec612e752804001",
             "PC 1.4 Xbox 360 controller rumble adapter and optional impulse triggers.", texture="GLOBAL/XtendedInputButtons.tpk", cursor=True),
        Game("UC", "UC", "fd0b42b2688de16917d360897c7da9baf25a7426f318b6ad57e79bc6e780ccc4",
             "Input support only. Rumble is not implemented."),
        Game("PS", "ProStreet", "15ad3e2996cb340aefcbd5c3eb43b7d3524c954657f17dd292fe9435efb9b01a",
             "Experimental native PC rumble adapter, with live surface/vehicle data and temporary MW effect tuning. Xbox 360 curves and controller validation are pending.", texture="GLOBAL/XtendedInputButtons.tpk", cursor=True),
        Game("W", "World", "4556330af357bc1118043a9de9392db90d040db58bdb0eee331f458cadb473f7",
             "Experimental upstream input support. Rumble is not implemented."),
    )
}
