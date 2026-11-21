"""Render figures directly from the saved GDB and QEMU logs."""
from pathlib import Path
import json
import textwrap
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parent
fonts = [Path('C:/Windows/Fonts/consola.ttf'),
         Path('/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf')]
font = ImageFont.truetype(str(next(path for path in fonts if path.exists())), 17)


def render(name, title, lines):
    rows = []
    for line in lines:
        rows.extend(textwrap.wrap(line, width=112, subsequent_indent='    ',
                    break_long_words=False, break_on_hyphens=False) or [''])
    image = Image.new('RGB', (1260, 112 + 26 * len(rows)), '#0d1523')
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, 1260, 64), fill='#16243a')
    draw.text((26, 22), title, font=font, fill='#e4edfc')
    for index, row in enumerate(rows):
        draw.text((26, 84 + index * 26), row, font=font,
                  fill='#86dfb1' if 'PASS' in row else '#c2d0e4')
    image.save(root / name)


comparison = []
for title, path in [('Initial non-leaf PTE', root / 'bootstrap-gdb.log'),
                    ('Corrected non-leaf PTE', root / 'after-fix/bootstrap-gdb.log')]:
    lines = path.read_text().splitlines()
    start = max(index for index, line in enumerate(lines) if line.startswith('pc '))
    comparison += [title, next(line for line in lines if line.startswith('$1 ='))]
    comparison += lines[start:start + 5] + ['']
render('A-01-nonleaf-gdb.png', 'Lab4 A / GDB log excerpts: non-leaf PTE', comparison)

lines = (root / 'qemu-128M.log').read_text().splitlines()
start = next(index for index, line in enumerate(lines) if line.startswith('check_alloc_page()'))
result = lines[start:] + ['', 'RAM configurations:']
for case in json.loads((root / 'summary.json').read_text())['cases']:
    result += [case['memory'] + ': ' + ('PASS' if case['passed'] else 'FAIL')]
render('A-02-verification.png', 'Lab4 A / QEMU log excerpts: verification', result)
