"""Translation catalogue of Project Foxy.

The program is written in Spanish: the Spanish text IS the key. QML uses qsTr("..."), C++ uses tr("...") /
core::tr("...") and the effect tables (names, sliders, presets...) are translated where they cross into the
interface. A translation is looked up by that Spanish text (see src/app/Translations.cpp).

    python tools/i18n_tool.py extract   -> prints every Spanish source string found in the code
    python tools/i18n_tool.py build     -> reads i18n/strings.tsv and writes resources/i18n/<lang>.json
    python tools/i18n_tool.py check     -> strings of the code that are missing in strings.tsv (and the unused ones)

i18n/strings.tsv: one line per string, tab separated: es <TAB> en <TAB> pt <TAB> ko <TAB> zh <TAB> ja
(`\\n` in a cell is a line break; an empty cell means "same as Spanish").
"""
import glob
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LANGS = ["en", "pt", "ko", "zh", "ja"]
STR = re.compile(r'"((?:[^"\\]|\\.)*)"')

# C++ files whose literals are the names of effects, sliders, looks, layouts... (translated where they reach the UI)
TABLE_FILES = ["src/core/edit/Effects*.cpp", "src/core/edit/Looks.cpp", "src/core/edit/Blend.cpp", "src/core/edit/Blend.h",
               "src/core/collage/Layouts.cpp", "src/core/lens/LensDatabase.cpp", "src/core/lens/LensDatabase.h"]
# C++ files whose messages are shown to the user (they go through tr()/core::tr() and are found by that call)
SKIP_EXACT = {"Generic", "calibrado en otro sensor", "Segoe UI", "Arial", "Impact", "Georgia", "Times New Roman", "Comic Sans MS", "Courier New", "Consolas"}


def unescape(s):
    return s.replace('\\"', '"').replace("\\n", "\n").replace("\\\\", "\\").replace("\\t", "\t")


def qml_strings():
    out = {}
    for f in sorted((ROOT / "qml").rglob("*.qml")):
        text = f.read_text(encoding="utf8")
        for m in re.finditer(r'qsTr\(\s*"((?:[^"\\]|\\.)*)"(?:\s*\+\s*"((?:[^"\\]|\\.)*)")*', text):
            out.setdefault(unescape(m.group(1)), f.name)
    return out


def cpp_marked():
    out = {}
    for f in sorted((ROOT / "src").rglob("*.*")):
        if f.suffix not in (".cpp", ".h"):
            continue
        text = f.read_text(encoding="utf8")
        for m in re.finditer(r'(?:\btr|\btr_|QT_TR_NOOP|QT_TRANSLATE_NOOP\(\s*"[^"]*"\s*,)\(?\s*(?:QStringLiteral\()?"((?:[^"\\]|\\.)*)"', text):
            out.setdefault(unescape(m.group(1)), f.name)
    return out


def table_strings():
    out = {}
    for pattern in TABLE_FILES:
        for f in sorted(ROOT.glob(pattern)):
            for n, line in enumerate(f.read_text(encoding="utf8").splitlines(), 1):
                s = line.strip()
                if s.startswith("//") or s.startswith("#include") or "QStringLiteral(\"" in s and "id" in s.split("QStringLiteral")[0][-6:]:
                    continue
                for m in STR.finditer(line):
                    t = unescape(m.group(1))
                    if t in SKIP_EXACT or not re.search(r"[A-Za-zÁ-ú]", t):
                        continue
                    if re.fullmatch(r"[a-z][A-Za-z0-9_.\-]*", t):   # an identifier ("caption", "stretchX")
                        continue
                    if t.startswith(("%", "#", "/", ".", "*", ":", "image/")) or "::" in t:
                        continue
                    out.setdefault(t, f.name)
    return out


def all_sources():
    out = {}
    for d in (qml_strings(), cpp_marked(), table_strings()):
        for k, v in d.items():
            out.setdefault(k, v)
    return out


def read_tsv():
    path = ROOT / "i18n" / "strings.tsv"
    rows = {}
    if not path.exists():
        return rows
    for n, line in enumerate(path.read_text(encoding="utf8").splitlines(), 1):
        if not line.strip() or line.startswith("#"):
            continue
        cells = line.split("\t")
        if len(cells) != 6:
            print(f"strings.tsv:{n}: expected 6 cells, found {len(cells)}: {line[:60]}", file=sys.stderr)
            continue
        rows[cells[0].replace("\\n", "\n")] = [c.replace("\\n", "\n") for c in cells[1:]]
    return rows


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "check"
    if cmd == "extract":
        for k, v in sorted(all_sources().items(), key=lambda kv: (kv[1], kv[0])):
            print(f"{v}\t{k!r}")
    elif cmd == "build":
        rows = read_tsv()
        out_dir = ROOT / "resources" / "i18n"
        out_dir.mkdir(parents=True, exist_ok=True)
        for i, lang in enumerate(LANGS):
            data = {es: cells[i] for es, cells in rows.items() if cells[i] and cells[i] != es}
            (out_dir / f"{lang}.json").write_text(json.dumps(data, ensure_ascii=False, indent=0, sort_keys=True), encoding="utf8")
            print(lang, len(data))
    elif cmd == "check":
        src, rows = all_sources(), read_tsv()
        missing = sorted(k for k in src if k not in rows)
        unused = sorted(k for k in rows if k not in src)
        for k in missing:
            print("MISSING\t" + src[k] + "\t" + repr(k))
        print(f"{len(src)} strings in the code, {len(rows)} in strings.tsv, {len(missing)} missing, {len(unused)} unused")
        if "--unused" in sys.argv:
            for k in unused:
                print("UNUSED\t" + repr(k))
        bad = 0
        for es, cells in rows.items():
            want = sorted(re.findall(r"%\d", es))
            for lang, cell in zip(LANGS, cells):
                if cell and sorted(re.findall(r"%\d", cell)) != want:
                    print(f"PLACEHOLDERS	{lang}	{es!r} -> {cell!r}")
                    bad += 1
        if bad:
            sys.exit(1)
        for lang_i, lang in enumerate(LANGS):
            empty = [k for k in rows if k in src and not rows[k][lang_i]]
            if empty:
                print(f"{lang}: {len(empty)} left empty (they show in Spanish)")
        sys.exit(1 if missing else 0)


if __name__ == "__main__":
    main()
