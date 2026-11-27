#!/usr/bin/env python3
"""Regression checks for a release build, against the last release's results (tools/release.sh runs them before
anything is published).

    tools/regression.py check  --exe <SuperOdysseyRecomp.exe> --out <new dir> [--log <file> ...] [--only a,b]
    tools/regression.py accept --out <that dir>     (after the release: its results become the next baseline)

Checks (each stops the release when it fails):
  shaders  no shader that fails to compile or translate, no draw skipped for a failed program, no fatal error - in
           every log of these runs and in the logs given with --log (release.sh's own runs). 1.0.3 shipped 20 broken
           shaders its release logs showed.
  pictures every kingdom on game 1.4.1 (and a few on 1.0.0) screenshotted by tools/kingdom_tour.py and compared with the
           last release's pictures; a kingdom that differs stops the release (the tour's report.html shows both).
  ride     90 s of running and turning in the kingdom the gate save starts in, on 8 logical processors (the PCs players
           reported stutter on: #177), 60 FPS: frames over 25 ms and the slowest tenth of the 2-second windows,
           compared with the last release (more than 1.5x the frames over 25 ms + 10, or 6 FPS lower, fails).
  saves    each save of GATE_SAVES (game 1.4.1) loads with Continue and reaches gameplay within 150 s.
  amd      (when GATE_AMD is set) the Sand Kingdom on the AMD graphics: the dust's ground shadow drawn soft, not as squares
           (#103) - the dark share of the frames under 0.015 (squares: ~0.05).

Machine paths (tools/release.local.sh exports them; never committed):
  GATE_141, GATE_100     game data roots (extracted/) for game 1.4.1 / 1.0.0 - copies for these tests only
  GATE_SAVES             a folder of saves, each <name>/save_0 (1.4.1)
  GATE_TOUR_SAVE_141, GATE_TOUR_SAVE_100   saves with every kingdom open (each holding save_0), for the tour
  GATE_RIDE_SAVE         optional: the save the ride starts from (default GATE_TOUR_SAVE_141)
  GATE_BASE              the last release's results (written by `accept`)
  GATE_AMD               optional: "<data root>;<gpu index>" - a 1.4.1 data root for the AMD graphics and its index
Test runs only: these data roots get the build's game code (rebuilt by its first start) and caches.
"""
import argparse, glob, json, os, re, shutil, subprocess, sys, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
BAD = [(re.compile(p), what) for p, what in (
    (r'GLSL compile failed', 'a shader failed to compile'),
    (r'translation failed|translate failed', 'a shader failed to translate'),
    (r'\[draw\] skipped \(program failed\)', 'a draw skipped: its program failed'),
    (r'program unavailable', 'a program with no shader code'),
    (r'^FATAL', 'a fatal error'),
    (r'^\*\*\* (exception|signal)', 'a crash'))]
VK = re.compile(r'\[vk\] t=([\d.]+) frame (\d+) ([\d.]+) fps max (\d+)ms')
HITCH = re.compile(r'\[hitch\] frame (\d+): (\d+) ms')
ARRIVAL = re.compile(r'\[warp\] stage scene \w+ running at frame (\d+) stage "([^"]+)"')
CONTINUE = '1500:A:8,1650:A:8,1800:A:8,1950:A:8,2100:A:8'
# (a save made on game 1.0.0, loaded on 1.4.1: the game shows its update notification pages first, each closed with A)
CONTINUE_LONG = ','.join(f'{f}:A:8' for f in range(1500, 4200, 150))


def env_path(name, need=True):
    v = os.environ.get(name, '')
    if need and not v:
        sys.exit(f'set {name} (tools/release.local.sh)')
    return Path(v) if v else None


def settings_file(out):  # (fixed settings: the player's own never apply to these runs)
    p = out / 'gate.ini'
    p.write_text('window_mode = windowed\nresolution = match\nvolume = 0\nframe_rate = 60\nframe_interp = 0\n'
                 'temporal_backend = dlss\ndlss_mode = off\nantialias = off\nupscaler = bilinear\nmotion = 0\n'
                 'vibration = 0\nmouse_camera = 0\nskip_intro = 1\ncheck_updates = 0\nconfig_at_start = 0\n',
                 encoding='ascii')
    return p


def run_game(exe, out, name, data, save, extra, until, timeout):
    """Starts the build on a copy of `save`; stops it once until(log text) is true or after timeout seconds."""
    saves = out / f'{name}-save'
    if saves.exists():
        shutil.rmtree(saves)
    shutil.copytree(save, saves)
    env = {k: v for k, v in os.environ.items() if not k.upper().startswith('ODYSSEY_')}
    env.update(ODYSSEY_DATA_ROOT=str(data), ODYSSEY_SAVE_DIR=str(saves), ODYSSEY_SETTINGS=str(settings_file(out)),
               ODYSSEY_NO_CONFIG='1', ODYSSEY_NO_UPDATE_CHECK='1', ODYSSEY_PREPARE='0', ODYSSEY_DISCORD='0',
               ODYSSEY_WINDOW='1280x720', ODYSSEY_LOG_STAGES='1')
    env.update(extra)
    log_path = out / f'{name}.log'
    t0 = time.monotonic()
    with open(log_path, 'wb') as log:
        proc = subprocess.Popen([str(exe)], cwd=str(exe.parent), env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            while proc.poll() is None and time.monotonic() - t0 < timeout:
                time.sleep(2)
                if until(log_path.read_text(encoding='utf-8', errors='replace')):
                    break
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.wait()
    return log_path.read_text(encoding='utf-8', errors='replace'), time.monotonic() - t0


def build_ready(exe, out, data, save, tag):
    """A first start of this build on a data root: its game code built (minutes) and the game reaching the title, so
    the timed runs after it don't include the build."""
    # (ODYSSEY_PREPARE=1: the shaders prepared here too, when the build's shaders changed - not during the timed runs)
    text, secs = run_game(exe, out, f'prepare-{tag}', data, save, {'ODYSSEY_HOST_INPUT': '600:quit:x', 'ODYSSEY_PREPARE': '1'},
                          lambda t: False, 3600)
    ok = '[vk] t=' in text
    return ok, f'{tag}: {"ready" if ok else "never reached the game"} after {secs:.0f} s'


def check_pictures(exe, out, base, version, data, save, kingdoms):
    tour = out / f'tour-{version}'
    cmd = [sys.executable, str(HERE / 'kingdom_tour.py'), '--exe', str(exe), '--data-root', str(data), '--save', str(save),
           '--out', str(tour), '--timeout', '420']
    if kingdoms:
        cmd += ['--kingdoms'] + kingdoms
    b = base / f'tour-{version}' if base else None
    if b and (b / 'report.json').is_file():
        cmd += ['--baseline', str(b)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    report = json.loads((tour / 'report.json').read_text(encoding='utf-8')) if (tour / 'report.json').is_file() else {}
    res = report.get('results', [])
    failed = [x['kingdom'] for x in res if x.get('status') != 'captured']
    review = [x['kingdom'] for x in res if x.get('comparison') in ('review', 'size_mismatch', 'missing_baseline')]
    if r.returncode == 1 or failed or not res:
        return False, f'{version}: kingdoms that didn\'t load: {", ".join(failed) or "(tour failed: " + r.stderr[-300:] + ")"}'
    if review:
        return False, f'{version}: pictures differ from the last release: {", ".join(review)} - see {tour / "report.html"}'
    first = not (b and (b / 'report.json').is_file())
    return True, f'{version}: {len(res)} kingdoms ' + ('captured (no baseline yet: these become it)' if first else 'match the last release')


def ride_input():
    parts = [CONTINUE]
    f, angle = 2600, 90
    while f < 8000:  # (run_input.py: 60-frame legs turning 40 degrees, the camera turned and a jump now and then)
        parts.append(f'{f}:STICK_{angle % 360}:60')
        if (f // 60) % 5 == 0:
            parts.append(f'{f}:RSTICK_RIGHT:40')
        if (f // 60) % 7 == 0:
            parts.append(f'{f + 20}:A:10')
        angle += 40
        f += 60
    return ','.join(parts)


def check_ride(exe, out, base, data, save):
    text, _ = run_game(exe, out, 'ride', data, save,
                       {'ODYSSEY_AFFINITY': '0xff', 'ODYSSEY_VK_PROF': '1', 'ODYSSEY_HITCH_MS': '25', 'ODYSSEY_INPUT': ride_input()},
                       lambda t: any(int(m.group(2)) >= 8000 for m in VK.finditer(t)), 900)
    arrivals = [int(m.group(1)) for m in ARRIVAL.finditer(text)]
    if not arrivals:
        return False, 'never reached gameplay', {}
    start = arrivals[0] + 300  # (after the arrival's own loading frames)
    # (a re-entry - a fall, a pipe - loads again: 300 frames around each left out)
    loading = lambda f: any(a - 30 <= f <= a + 300 for a in arrivals)
    hitches = [int(m.group(2)) for m in HITCH.finditer(text) if start < int(m.group(1)) < 8000 and not loading(int(m.group(1)))]
    fps = sorted(float(m.group(3)) for m in VK.finditer(text)
                 if start < int(m.group(2)) <= 8000 and not loading(int(m.group(2))))
    if len(fps) < 20:
        return False, f'the ride was too short ({len(fps)} windows)', {}
    m = {'hitches_25': len(hitches), 'hitches_50': sum(h > 50 for h in hitches), 'p10_fps': fps[len(fps) // 10],
         'worst_ms': max(hitches, default=0)}
    (out / 'ride.json').write_text(json.dumps(m, indent=1), encoding='utf-8')
    b = base / 'ride.json' if base else None
    if not (b and b.is_file()):
        return True, f'{m} (no baseline yet)', m
    bm = json.loads(b.read_text(encoding='utf-8'))
    bad = m['hitches_25'] > bm['hitches_25'] * 1.5 + 10 or m['p10_fps'] < bm['p10_fps'] - 6
    return not bad, f'{m}, last release {bm}', m


def check_saves(exe, out, data, saves_dir):
    msgs, ok = [], True
    for s in sorted(p for p in saves_dir.iterdir() if (p / 'save_0').is_dir()):
        text, secs = run_game(exe, out, f'save-{s.name}', data, s, {'ODYSSEY_INPUT': CONTINUE_LONG},
                              lambda t: any(int(m.group(1)) > 1500 for m in ARRIVAL.finditer(t)), 150)
        arrived = [m for m in ARRIVAL.finditer(text) if int(m.group(1)) > 1500]
        good = bool(arrived)
        ok &= good
        msgs.append(f'{s.name}: ' + (f'{arrived[0].group(2)} after {secs:.0f} s' if good else f'no gameplay within {secs:.0f} s'))
    return ok, '; '.join(msgs)


def darkfrac(frames):
    from PIL import Image
    import numpy as np
    vals = []
    for f in sorted(glob.glob(str(frames / '*.png')))[10:20]:
        a = np.asarray(Image.open(f).convert('RGB')).astype(int)
        low = a[a.shape[0] // 2:]
        vals.append(((low[..., 0] < 120) & (low[..., 1] < 60)).mean())
    return sum(vals) / len(vals) if vals else None


def check_amd(exe, out, spec):
    data, gpu = spec.split(';')
    data = Path(data)
    frames = data / 'build' / 'frames'
    shutil.rmtree(frames, ignore_errors=True)
    (data / 'build').mkdir(exist_ok=True)
    parts = [CONTINUE] + [f'{f}:STICK_{(90 + (f - 4200) // 60 * 40) % 360}:60' for f in range(4200, 8400, 60)]
    run_game(exe, out, 'amd', data, data / 'save', {
        'ODYSSEY_GPU': gpu, 'ODYSSEY_WARP': 'world:SandWorldHomeStage@2400', 'ODYSSEY_PIPELINE_PREWARM': '1',
        'ODYSSEY_INPUT': ','.join(parts), 'ODYSSEY_SCREENSHOT_EVERY': '30', 'ODYSSEY_SCREENSHOT_FROM': '4300'},
        lambda t: len(glob.glob(str(frames / '*.png'))) >= 20, 900)
    d = darkfrac(frames)
    if d is None:
        return False, 'no frames from the AMD graphics'
    return d < 0.015, f'dark share {d:.4f} (squares ~0.05, right ~0.004)'


def check(args):
    try:
        import numpy, PIL  # noqa: F401 (the tour and the AMD check need them)
    except ImportError:
        sys.exit('this Python has no numpy / Pillow: pip install numpy pillow')
    exe, out = args.exe.resolve(), args.out.resolve()
    out.mkdir(parents=True)
    base = env_path('GATE_BASE', False)
    only = set(args.only.split(',')) if args.only else None
    want = lambda k: not only or k in only
    g141, g100 = env_path('GATE_141'), env_path('GATE_100')
    t141, t100 = env_path('GATE_TOUR_SAVE_141'), env_path('GATE_TOUR_SAVE_100')
    results = []
    def note(name, ok, msg):
        ok = bool(ok)
        results.append({'check': name, 'ok': ok, 'detail': msg})
        print(f'{"ok  " if ok else "FAIL"} {name}: {msg}', flush=True)
    ok, msg = build_ready(exe, out, g141, t141, '1.4.1'); note('build 1.4.1', ok, msg)
    if want('pictures'):
        ok, msg = build_ready(exe, out, g100, t100, '1.0.0'); note('build 1.0.0', ok, msg)
    if want('pictures'):
        note('pictures', *check_pictures(exe, out, base, '1.4.1', g141, t141, None))
        note('pictures', *check_pictures(exe, out, base, '1.0.0', g100, t100, ['cap', 'cascade', 'sand', 'metro', 'seaside', 'moon']))
    if want('ride'):
        ok, msg, _ = check_ride(exe, out, base, g141, env_path('GATE_RIDE_SAVE', False) or t141); note('ride', ok, msg)
    if want('saves'):
        note('saves', *check_saves(exe, out, g141, env_path('GATE_SAVES')))
    amd = os.environ.get('GATE_AMD')
    if amd and want('amd'):
        note('amd', *check_amd(exe, out, amd))
    # last: every log of these runs and the release's own
    found = []
    for f in list(out.rglob('*.log')) + [Path(p) for p in args.log or []]:
        try:
            lines = Path(f).read_text(encoding='utf-8', errors='replace').splitlines()
        except OSError:
            continue
        for pat, what in BAD:
            n = sum(1 for line in lines if pat.search(line))
            if n:
                found.append(f'{Path(f).name}: {what} ({n})')
    note('shaders', not found, '; '.join(found) if found else 'no failed shader, skipped program or fatal error in any log')
    (out / 'results.json').write_text(json.dumps(results, indent=1), encoding='utf-8')
    bad = [r for r in results if not r['ok']]
    print(f'\n{"REGRESSION CHECKS FAILED: " + ", ".join(sorted({r["check"] for r in bad})) if bad else "all regression checks passed"}')
    return 1 if bad else 0


def accept(args):
    out, base = args.out.resolve(), env_path('GATE_BASE')
    base.mkdir(parents=True, exist_ok=True)
    for name in ('tour-1.4.1', 'tour-1.0.0'):
        if (out / name / 'report.json').is_file():
            shutil.rmtree(base / name, ignore_errors=True)
            shutil.copytree(out / name, base / name, ignore=shutil.ignore_patterns('*-save', '*.log', '*-diff.png'))
    if (out / 'ride.json').is_file():
        shutil.copy2(out / 'ride.json', base / 'ride.json')
    print(f'baseline: {base}')
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    c = sub.add_parser('check')
    c.add_argument('--exe', type=Path, required=True)
    c.add_argument('--out', type=Path, required=True)
    c.add_argument('--log', action='append', help='a log of the release\'s own runs, checked for failed shaders too')
    c.add_argument('--only', help='comma list: pictures,ride,saves,amd (the shader check always runs)')
    a = sub.add_parser('accept')
    a.add_argument('--out', type=Path, required=True)
    args = ap.parse_args()
    return check(args) if args.cmd == 'check' else accept(args)


if __name__ == '__main__':
    raise SystemExit(main())
