"""Verify the local retail Undercover XexTool memory image (no game files bundled)."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def execute_begin(data, port):
    """Execute the retail BeginUpdate integer/load/store subset, bounded to 256 steps.

    Float loads/stores move bits only. No reconstructed phase-reset formula is
    used here: the returned writes are produced by the executable instructions.
    """
    regs = [0]*32
    floats = [0]*32
    regs[3] = 0x100100
    memory = {regs[3]-8: port}
    writes = {}
    pc = 0x82610A48
    equal = False

    def signed16(value):
        return value-0x10000 if value & 0x8000 else value

    def read(address):
        if address in memory:
            return memory[address]
        return struct.unpack_from(">I", data, address-0x82000000)[0]

    for _ in range(256):
        word = read(pc)
        op, dest, src, imm = word >> 26, (word >> 21) & 31, (word >> 16) & 31, word & 0xFFFF
        immediate = signed16(imm)
        following = pc+4
        if word == 0x4E800020:  # blr
            return writes
        if op in (14, 15):  # addi, addis; r0 means literal zero
            regs[dest] = ((regs[src] if src else 0)+immediate*(65536 if op == 15 else 1)) & 0xFFFFFFFF
        elif op == 7:  # mulli
            regs[dest] = (regs[src]*immediate) & 0xFFFFFFFF
        elif op == 32:  # lwz
            regs[dest] = read((regs[src]+immediate) & 0xFFFFFFFF)
        elif op == 48:  # lfs (bit-preserving)
            floats[dest] = read((regs[src]+immediate) & 0xFFFFFFFF)
        elif op == 52:  # stfs
            address = (regs[src]+immediate) & 0xFFFFFFFF
            memory[address] = writes[address] = floats[dest]
        elif op == 31 and (word >> 1) & 0x3FF == 266:  # add
            regs[dest] = (regs[src]+regs[(word >> 11) & 31]) & 0xFFFFFFFF
        elif op == 13:  # addic.; this loop branches on CR0 EQ
            regs[dest] = (regs[src]+immediate) & 0xFFFFFFFF
            equal = regs[dest] == 0
        elif op == 16 and dest == 4 and src == 2:  # bne
            if not equal:
                following = pc+signed16(word & 0xFFFC)
        else:
            raise ValueError(f"Unsupported BeginUpdate instruction {word:08X} at {pc:08X}")
        pc = following
    raise ValueError("BeginUpdate exceeded instruction bound")


def audit(path):
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    expected = "befa136b20e47fc62b8129ccb32b97e6163ddb0bff52760d532788fc3e925c71"
    if digest != expected:
        raise ValueError("Unsupported Xbox reference image: SHA-256 " + digest)

    def word(va):
        return struct.unpack_from(">I", data, va - 0x82000000)[0]

    def check(va, expected_word):
        actual = word(va)
        if actual != expected_word:
            raise ValueError(f"0x{va:08X}: expected {expected_word:08X}, got {actual:08X}")

    table = struct.unpack_from(">21I", data, 0x12CC70)
    # Read the literal table entry as a VA, without the stale PE raw offset.
    assert table[4] == 0x82610A48
    check(table[4], 0x8143FFF8)  # lwz r10,-8(r3), BeginUpdate
    check(table[9], 0x4E800020)  # RPM is blr
    check(0x826112E8, 0xD14B0114)  # stereo drift amplitude: stfs f10,0x114(r11)
    check(0x826112FC, 0xD16B0110)  # phase: stfs f11,0x110(r11)
    # Authored collision path: fixed wheel envelope is a different branch.
    check(0x82612024, 0x480058ED)  # bl EvaluateWaveRecord (0x82617910)

    constants = {
        "one": (0x820D9B14, 1.0),
        "drift_amplitude_gain": (0x820DA074, 0.5),
        "drift_amplitude_base": (0x820DA154, 0.2),
        "drift_frequency_gain": (0x820DA278, 30.0),
        "drift_frequency_base": (0x820DA000, 60.0),
        "mph_per_mps": (0x82167AFC, 2.23699),
        "rear_contact_gain": (0x820D6788, 0.25),
        "projection_threshold": (0x820D77BC, 0.1),
    }
    values = {}
    for name, (va, expected_value) in constants.items():
        value = struct.unpack_from(">f", data, va - 0x82000000)[0]
        if abs(value - expected_value) > 1e-6:
            raise ValueError(f"{name}: expected {expected_value}, got {value}")
        values[name] = value
    groups = (0x000, 0x040, 0x080, 0x100, 0x140, 0x180)
    for port in (0, 1, 3):
        base = 0x8312B8E8+port*0x274
        writes = execute_begin(data, port)
        expected_writes = {base+group+offset: 0 for group in groups for offset in range(0, 64, 4)}
        expected_writes[base+0x268] = 0x3F800000
        if writes != expected_writes:
            raise ValueError("Native BeginUpdate reset map differs")
    return {"sha256": digest, "mapping": "file offset = VA - 0x82000000",
            "feedback_vtable": [f"0x{va:08X}" for va in table], "constants": values,
            "begin_update": {"wave_groups_cleared_including_phase": [f"0x{x:03X}" for x in groups],
                             "ramps_preserved": True, "ports_executed": [0, 1, 3]}}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    args = parser.parse_args()
    print(json.dumps(audit(args.image), indent=2))
