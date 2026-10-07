"""Appends rows to i18n/strings.tsv:  python tools/i18n_add.py file.txt   (same line format as i18n/parts: `Spanish || en || pt || ko || zh || ja`)."""
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
tsv = root / "i18n" / "strings.tsv"
existing = {l.split("\t")[0] for l in tsv.read_text(encoding="utf8").splitlines() if l.strip()}
added = []
for line in Path(sys.argv[1]).read_text(encoding="utf8").splitlines():
    if not line.strip():
        continue
    cells = [c.strip() for c in line.split(" || ")]
    if len(cells) != 6:
        sys.exit("bad line: " + line[:70])
    if cells[0] in existing:
        print("already there:", cells[0][:50])
        continue
    added.append("\t".join(c.replace("\n", "\n") for c in cells))
with open(tsv, "a", encoding="utf8", newline="") as f:
    f.write("\n".join(added) + ("\n" if added else ""))
print(len(added), "added")
