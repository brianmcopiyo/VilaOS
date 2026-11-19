#!/usr/bin/env python3
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

fixes = {
    'text="Photos/ dir"': 'text="Photos/  dir"',
    'text="Notes/ dir"': 'text="Notes/  dir"',
    'text="readme.txt 2 KB"': 'text="readme.txt  2 KB"',
    'text="budget.csv 8 KB"': 'text="budget.csv  8 KB"',
    'text="voice01.amr 24 KB"': 'text="voice01.amr  24 KB"',
}
for rel in [
    "apps/files/res/layout/main.xml",
    "apps/files/ui/main.xml",
    "assets/screens/files.xml",
]:
    p = ROOT / rel
    t = p.read_text(encoding="utf-8")
    for a, b in fixes.items():
        t = t.replace(a, b)
    p.write_text(t, encoding="utf-8", newline="\n")

pats = [
    re.compile(r'(?<![a-z-])fg='),
    re.compile(r'(?<![a-z-])bg='),
    re.compile(r'(?<![a-z-])\bw='),
    re.compile(r'(?<![a-z-])\bh='),
    re.compile(r'focus_bg='),
    re.compile(r'left_action='),
    re.compile(r'center_action='),
    re.compile(r'right_action='),
    re.compile(r'max_len='),
    re.compile(r'min_h='),
    re.compile(r'max_h='),
    re.compile(r'(?<![a-z-])border='),
]
bad = []
for folder in ("assets", "apps", "sdk"):
    base = ROOT / folder
    if not base.exists():
        continue
    for p in base.rglob("*.xml"):
        t = p.read_text(encoding="utf-8")
        for pat in pats:
            if pat.search(t):
                bad.append(f"{p.relative_to(ROOT)} :: {pat.pattern}")
print("remaining", len(bad))
for b in bad[:30]:
    print(b)
