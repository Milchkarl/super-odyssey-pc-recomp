"""Draws the progress bars of docs/STATUS.md and the README: python tools/status_bars.py

Reads docs/STATUS.md: each "### <Area> (<share>%)" heading starts an area; each table row with a
"![NN%](media/progress/NN.svg)" bar and a "xN" weight is a part. An area's value is the weighted mean of its parts,
the overall value the weighted mean of the areas by their shares. Writes docs/media/progress/NN.svg for every value
used, area-<name>.svg for each area and overall.svg, and prints the numbers (put them in the README's table by hand:
the bars update by themselves).
"""
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'docs', 'media', 'progress')


def colour(p):
    return '#3fb950' if p >= 90 else '#58a6ff' if p >= 70 else '#d29922' if p >= 40 else '#8b949e'


def svg(pct, label, width, height=20, size=11):
    fill = round(width * pct / 100)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" '
            f'role="img" aria-label="{label}"><rect width="{width}" height="{height}" rx="4" fill="#30363d"/>'
            f'<rect width="{fill}" height="{height}" rx="4" fill="{colour(pct)}"/>'
            f'<text x="{width / 2:g}" y="{height * 0.7:g}" text-anchor="middle" '
            f'font-family="Verdana,DejaVu Sans,sans-serif" font-size="{size}" fill="#fff">{label}</text></svg>\n')


def write(name, text):
    with open(os.path.join(OUT, name), 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)


def main():
    os.makedirs(OUT, exist_ok=True)
    areas, cur = [], None
    for line in open(os.path.join(ROOT, 'docs', 'STATUS.md'), encoding='utf-8'):
        m = re.match(r'### (.+?) \((\d+)%\)', line)
        if m:
            cur = {'name': m.group(1), 'share': int(m.group(2)), 'parts': []}
            areas.append(cur)
            continue
        m = re.search(r'!\[(\d+)%\]\(media/progress/\d+\.svg\)\s*\|\s*x(\d)', line)
        if m and cur:
            cur['parts'].append((int(m.group(1)), int(m.group(2))))
    used, total, shares = set(), 0.0, 0
    for a in areas:
        w = sum(x for _, x in a['parts'])
        a['value'] = round(sum(p * x for p, x in a['parts']) / w) if w else 0
        used.update(p for p, _ in a['parts'])
        slug = re.sub(r'[^a-z0-9]+', '-', a['name'].lower()).strip('-')
        write(f'area-{slug}.svg', svg(a['value'], f"{a['value']}%", 160))
        total += a['value'] * a['share']
        shares += a['share']
        print(f"{a['name']}: {a['value']}% (share {a['share']}%, {len(a['parts'])} parts)")
    for p in sorted(used):
        write(f'{p}.svg', svg(p, f'{p}%', 110))
    overall = round(total / shares) if shares else 0
    write('overall.svg', svg(overall, f'{overall}% overall', 420, 28, 13))
    print(f'overall: {overall}% (shares add up to {shares}%)')


if __name__ == '__main__':
    main()
