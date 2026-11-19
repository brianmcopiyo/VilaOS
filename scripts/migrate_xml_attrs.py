#!/usr/bin/env python3
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

REPLACEMENTS = [
    ("left_action=", "left-action="),
    ("center_action=", "center-action="),
    ("right_action=", "right-action="),
    ("focus_bg=", ""),
    ("border_color=", "border-color="),
    ("margin_top=", "margin-top="),
    ("margin_right=", "margin-right="),
    ("margin_bottom=", "margin-bottom="),
    ("margin_left=", "margin-left="),
    ("padding_top=", "padding-top="),
    ("padding_right=", "padding-right="),
    ("padding_bottom=", "padding-bottom="),
    ("padding_left=", "padding-left="),
    ("row_gap=", "row-gap="),
    ("col_gap=", "col-gap="),
    ("min_w=", "min-width="),
    ("min_h=", "min-height="),
    ("max_w=", "max-width="),
    ("max_h=", "max-height="),
    ("col_span=", "span="),
    ("row_span=", "span="),
    ("flex_direction=", "direction="),
]

ATTR_MAP = {
    "bg": "background",
    "fg": "color",
    "w": "width",
    "h": "height",
}

FOCUS_BG_RE = re.compile(r'\s+focus_bg="[^"]*"')
MAX_LEN_RE = re.compile(r'\s+max_len="[^"]*"')
BORDER_RE = re.compile(r'(^|[\s/])border="')
ATTR_RE = re.compile(r'(^|[\s/])(bg|fg|w|h)="')


def rewrite_line(line: str) -> str:
    line = FOCUS_BG_RE.sub("", line)
    line = MAX_LEN_RE.sub("", line)
    for old, new in REPLACEMENTS:
        line = line.replace(old, new)
    line = BORDER_RE.sub(r'\1border-width="', line)

    def repl_attr(m):
        prefix, name = m.group(1), m.group(2)
        return f'{prefix}{ATTR_MAP[name]}="'

    line = ATTR_RE.sub(repl_attr, line)
    return line


def migrate_file(path: Path) -> bool:
    raw = path.read_text(encoding="utf-8")
    lines = raw.splitlines(keepends=True)
    out = [rewrite_line(l) for l in lines]
    text = "".join(out)
    if text != raw:
        path.write_text(text, encoding="utf-8", newline="\n")
        return True
    return False


def main():
    roots = [
        ROOT / "assets",
        ROOT / "apps",
        ROOT / "sdk",
    ]
    n = 0
    for root in roots:
        if not root.exists():
            continue
        for p in root.rglob("*.xml"):
            if migrate_file(p):
                print(p.relative_to(ROOT))
                n += 1
    print(f"updated {n} files")


if __name__ == "__main__":
    main()
