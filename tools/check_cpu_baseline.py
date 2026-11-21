#!/usr/bin/env python3
"""Checks that a program runs on a CPU with AVX but without AVX2 (Intel Sandy / Ivy Bridge, AMD FX before
Excavator): no instruction such a CPU lacks may run unconditionally.

usage: tools/check_cpu_baseline.py <exe, dll, .o or .lib> ... [--dispatched <static library>] ...

Fails (exit 1) on FMA, F16C, AVX2 vector instructions (integer AVX on ymm, broadcasts from registers, gathers,
permutes across lanes), AVX-512, MOVBE, RDRAND / RDSEED, ADX, and on BMI1 / BMI2 / LZCNT beyond what the
--dispatched libraries bring. Those (libzstd.a: its *_bmi2 functions) pick the BMI2 code after their own CPUID
check, and their functions' names are gone from the linked exe, so their counts per instruction are allowed instead.
TZCNT is allowed: it is REP BSF, which an older CPU runs as BSF, and compilers emit it only where the two agree.

Why: the game code library is compiled on the player's machine for the CPU it has (runtime/gamelib.c), and zig's
runtime is checked by tools/zig_slim.sh, but nothing checked the exe and what is linked into it. A -mavx2 / -march
flag or an AVX2 helper in the runtime would crash every AVX-only player at start (docs/CPU-BASELINE.md).
OBJDUMP=<path> picks the objdump (default: objdump on PATH).
"""
import collections
import os
import re
import subprocess
import sys

BAD = re.compile(
    r'^(vfn?m(add|sub)\w*|vfmaddsub\w*|vfmsubadd\w*'  # FMA
    r'|vcvtph2ps|vcvtps2ph'  # F16C
    r'|vpbroadcast\w|vbroadcasti128|vperm2i128|vinserti128|vextracti128|vpermq|vpermd|vpermpd|vpermps'
    r'|v(p)?gather\w+|vpmaskmov\w|vpsllv\w|vpsrlv\w|vpsrav\w|vpblendd'  # AVX2
    r'|movbe|rdrand|rdseed|adcx|adox|xbegin|xend|xabort|clflushopt|clwb)$')
BMI = re.compile(r'^(andn|bextr|blsi|blsmsk|blsr|bzhi|mulx|pdep|pext|rorx|sarx|shlx|shrx|lzcnt)$')
# AVX (1) instructions that start with vp but are fine on ymm: moves, vptest, the float permutes
YMM_OK = re.compile(r'^(vmovdq[au]|vmovntdqa?|vptest|vlddqu|vperm2f128|vpermilp[sd])$')
PREFIXES = {'rep', 'repz', 'repe', 'repnz', 'repne', 'lock', 'notrack', 'bnd', 'data16', 'cs', 'ds', 'es', 'ss'}


def scan(path):
    """{function: Counter(mnemonic)} of the instructions an AVX-only CPU lacks, BMI included."""
    objdump = os.environ.get('OBJDUMP', 'objdump')
    out = subprocess.run([objdump, '-d', '--no-show-raw-insn', path], capture_output=True, text=True,
                         errors='replace', check=True).stdout
    hits = collections.defaultdict(collections.Counter)
    func = '?'
    for line in out.splitlines():
        m = re.match(r'^[0-9a-f]+ <(.+)>:', line)
        if m:
            func = m.group(1)
            continue
        parts = line.split('\t')
        if len(parts) < 2 or not parts[1].strip():
            continue
        words = parts[1].split()
        mn = words[0]
        if mn in PREFIXES and len(words) > 1:
            mn = words[1]
        ins = parts[1]
        if (BAD.match(mn) or BMI.match(mn) or '%zmm' in ins or re.match(r'^k(mov|and|or|xor|not|shift|test|unpck|add)', mn)
                or (mn.startswith('vp') and '%ymm' in ins and not YMM_OK.match(mn))):
            hits[func][mn] += 1
    return hits


def main(argv):
    files, dispatched = [], []
    i = 0
    while i < len(argv):
        if argv[i] == '--dispatched' and i + 1 < len(argv):
            dispatched.append(argv[i + 1])
            i += 2
        else:
            files.append(argv[i])
            i += 1
    if not files:
        print(__doc__)
        return 2
    allowed = collections.Counter()
    for lib in dispatched:
        for c in scan(lib).values():
            allowed.update({mn: n for mn, n in c.items() if BMI.match(mn)})
    ok = True
    for path in files:
        hits = scan(path)
        total = collections.Counter()
        for c in hits.values():
            total.update(c)
        over = {mn: n for mn, n in total.items() if not (BMI.match(mn) and n <= allowed[mn])}
        if over:
            ok = False
            print(f'{path}: instructions an AVX-only CPU (no AVX2 / FMA / BMI) lacks: {over}')
            for func, c in sorted(hits.items(), key=lambda x: -sum(x[1].values()))[:15]:
                bad = {mn: n for mn, n in c.items() if mn in over}
                if bad:
                    print(f'  {func}: {bad}')
        else:
            note = f' (BMI from the dispatched libraries: {dict(total)})' if total else ''
            print(f'{path}: ok for a CPU with AVX and without AVX2{note}')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
