#!/usr/bin/env python3
"""The game functions that run, for the game code build (runtime/gamebuild.c: a generated file defining none of them
is compiled without optimization). Addresses only - no game code.

    tools/hot_functions.py <exefs folder> <fnprof.txt> [<fnprof.txt> ...]

Each fnprof.txt is the function profiler's output (ODYSSEY_FNPROF=0:<frame>, ODYSSEY_FNPROF_OUT): "<addr> <count>"
per function entered. Use runs that go everywhere - every kingdom (ODYSSEY_WARP=world:<stage>@<frame>;...), moving
about, a recorded ride - of one game version; the exefs folder is that version's (its modules' build ids name the
sections). The sections for those modules and build ids in runtime/hle/hot_functions.txt are replaced; other versions'
are kept. A function missing from the list only makes its file slower to run (no optimization), never wrong.
"""
import os
import sys

LIST = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "runtime", "hle", "hot_functions.txt")
# (the modules' load addresses: runtime/gamebuild.c MODULES)
RANGES = [("main", 0x7000000000, 0x7003000000), ("subsdk0", 0x7003000000, 0x7004000000), ("sdk", 0x7004000000, 0x7005000000)]


def build_id(path):
    with open(path, "rb") as f:
        f.seek(0x40)
        return f.read(8).hex()


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    exefs, profiles = sys.argv[1], sys.argv[2:]
    ids = {m: build_id(os.path.join(exefs, m)) for m, _, _ in RANGES if os.path.exists(os.path.join(exefs, m))}
    hot = {m: set() for m in ids}
    for p in profiles:
        for line in open(p):
            parts = line.split()
            if len(parts) < 2:
                continue
            a = int(parts[0], 16)
            for m, lo, hi in RANGES:
                if m in hot and lo <= a < hi:
                    hot[m].add(a)
    sections, cur = {}, None
    if os.path.exists(LIST):
        for line in open(LIST):
            line = line.rstrip("\n")
            if line.startswith("# ") and len(line.split()) == 3:
                cur = tuple(line.split()[1:])
                sections[cur] = []
            elif cur and line and not line.startswith("#"):
                sections[cur].append(line)
    for m, i in ids.items():
        sections[(m, i)] = ["%x" % a for a in sorted(hot[m])]
        print(f"{m} {i}: {len(hot[m])} functions that run")
    with open(LIST, "w", newline="\n") as f:
        f.write("# The game functions that run, per module and game build id (tools/hot_functions.py; addresses only).\n")
        f.write("# runtime/gamebuild.c compiles a generated file that defines none of these without optimization.\n")
        for (m, i) in sorted(sections):
            f.write(f"# {m} {i}\n")
            for a in sections[(m, i)]:
                f.write(a + "\n")


if __name__ == "__main__":
    main()
