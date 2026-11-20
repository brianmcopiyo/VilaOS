#!/usr/bin/env python3
import os
import re
import sys
import html
import xml.etree.ElementTree as ET

TAGS = {
    "Dashboard", "Taskbar", "Window", "Panel", "Grid", "Stack", "Card", "Button",
    "Row", "Badge", "Input", "List", "Text", "Icon", "Keypad", "Tile", "Header",
    "Footer", "Screen", "screen", "flex", "label", "statusbar", "softkeys", "vbox",
    "hbox", "div",
}

ATTR_MAP = {
    "Background": "background",
    "Foreground": "color",
    "BorderBrush": "borderColor",
    "BorderThickness": "borderWidth",
    "CornerRadius": "borderRadius",
    "Padding": "padding",
    "Margin": "margin",
    "Spacing": "gap",
    "Columns": "columns",
    "Rows": "rows",
    "Orientation": "orientation",
    "FontSize": "fontSize",
    "FontWeight": "fontWeight",
    "Width": "width",
    "Height": "height",
    "Grow": "grow",
    "Content": "content",
    "Text": "text",
    "Title": "title",
    "Subtitle": "subtitle",
    "Placeholder": "placeholder",
    "Action": "action",
    "ColumnSpacing": "columnGap",
    "RowSpacing": "rowGap",
    "GridColumn": "gridColumn",
    "GridColumnSpan": "gridColumnSpan",
    "GridRow": "gridRow",
    "GridRowSpan": "gridRowSpan",
    "VerticalAlignment": "alignSelf",
    "HorizontalAlignment": "justifySelf",
    "x:Name": "id",
    "Name": "id",
    "id": "id",
    "class": "className",
    "background": "background",
    "color": "color",
    "text": "text",
    "icon": "icon",
    "action": "action",
    "width": "width",
    "height": "height",
    "radius": "borderRadius",
    "font-size": "fontSize",
    "font-weight": "fontWeight",
    "font-family": "fontFamily",
    "component": "component",
    "align-items": "alignItems",
    "x": "x",
    "y": "y",
}

TAG_ALIAS = {
    "screen": "Screen",
    "label": "Text",
    "flex": "Stack",
    "vbox": "Stack",
    "hbox": "Stack",
    "statusbar": "Header",
    "softkeys": "Footer",
    "div": "Panel",
}


def local(tag):
    if tag is None:
        return ""
    if "}" in tag:
        tag = tag.rsplit("}", 1)[-1]
    return tag


def css_len(v):
    if v is None:
        return None
    s = str(v).strip()
    if not s:
        return s
    if s.lower() in ("fill", "auto", "wrap", "*"):
        return s
    if s.endswith("%") or s.endswith("px"):
        return s
    if re.fullmatch(r"-?\d+(\.\d+)?", s):
        return s + "px"
    if "," in s:
        parts = [css_len(p.strip()) for p in s.split(",")]
        return " ".join(p for p in parts if p)
    return s


def prop_value(key, raw):
    if key in ("background", "color", "borderColor", "fontFamily", "className", "id",
               "action", "content", "text", "title", "subtitle", "placeholder", "icon",
               "component", "orientation", "columns", "rows", "alignSelf", "justifySelf",
               "alignItems", "fontWeight"):
        return raw
    if key in ("borderWidth", "borderRadius", "padding", "margin", "gap", "columnGap",
               "rowGap", "fontSize", "width", "height", "grow", "gridColumn", "gridColumnSpan",
               "gridRow", "gridRowSpan", "x", "y"):
        return css_len(raw)
    return raw


def esc(s):
    return s.replace("\\", "\\\\").replace("'", "\\'").replace("\n", "\\n")


def emit_node(el, indent=2):
    tag = local(el.tag)
    if tag.startswith("?"):
        return ""
    react_tag = TAG_ALIAS.get(tag, tag)
    if react_tag[0].islower() and react_tag not in ("Stack",):
        react_tag = TAG_ALIAS.get(tag, tag[:1].upper() + tag[1:] if tag else "Panel")
    props = []
    for k, v in el.attrib.items():
        lk = k
        if k.startswith("{") and "}" in k:
            lk = k.rsplit("}", 1)[-1]
        if lk.startswith("xmlns"):
            continue
        mapped = ATTR_MAP.get(lk) or ATTR_MAP.get(k)
        if not mapped:
            if lk.lower() == "name" or k.endswith("}Name"):
                mapped = "id"
            else:
                mapped = re.sub(r"[^A-Za-z0-9]", "", lk)
                if mapped:
                    mapped = mapped[0].lower() + mapped[1:]
                else:
                    continue
        val = prop_value(mapped, v)
        if val is None:
            continue
        if isinstance(val, str) and (val.isdigit() or re.fullmatch(r"-?\d+\.\d+", val or "")):
            props.append(f"{mapped}: {val}")
        else:
            props.append(f"{mapped}: '{esc(str(val))}'")
    pad = " " * indent
    children = []
    if el.text and el.text.strip():
        children.append(f"{pad}  '{esc(el.text.strip())}'")
    for child in list(el):
        chunk = emit_node(child, indent + 2)
        if chunk:
            children.append(chunk)
        if child.tail and child.tail.strip():
            children.append(f"{pad}  '{esc(child.tail.strip())}'")
    prop_s = ", ".join(props)
    if not children:
        return f"{pad}h({react_tag}, {{{prop_s}}})"
    inner = ",\n".join(children)
    return f"{pad}h({react_tag}, {{{prop_s}}},\n{inner}\n{pad})"


def module_name(path):
    base = os.path.splitext(os.path.basename(path))[0]
    safe = re.sub(r"[^A-Za-z0-9_]", "_", base)
    if not safe or safe[0].isdigit():
        safe = "View_" + safe
    return safe[0].upper() + safe[1:]


def transpile_file(src, dst):
    raw = open(src, "r", encoding="utf-8").read()
    raw = re.sub(r"<\?xml[^?]*\?>", "", raw).strip()
    raw = re.sub(r"\bx:Name=", "Name=", raw)
    raw = re.sub(r"\bxmlns:x=\"[^\"]*\"", "", raw)
    if "xmlns:x=" not in raw and re.search(r"\bx:[A-Za-z]+=", raw):
        raw = re.sub(r"(\sxmlns=\"[^\"]*\")", r'\1 xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"', raw, count=1)
    try:
        root = ET.fromstring(raw)
    except ET.ParseError as e:
        raise SystemExit(f"xaml2react parse error {src}: {e}")
    name = module_name(src)
    body = emit_node(root, 2)
    out = (
        f"(function(g){{\n"
        f"  function {name}(props){{\n"
        f"    return (\n{body}\n    );\n"
        f"  }}\n"
        f"  g.XamlViews = g.XamlViews || {{}};\n"
        f"  g.XamlViews.{name} = {name};\n"
        f"}})(window);\n"
    )
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    open(dst, "w", encoding="utf-8", newline="\n").write(out)
    return dst


def transpile_tree(src_dir, out_dir):
    count = 0
    for root, _dirs, files in os.walk(src_dir):
        for fn in files:
            if not (fn.endswith(".xaml") or fn.endswith(".xml")):
                continue
            if fn in ("manifest.xml", "colors.xml", "dimens.xml"):
                continue
            sp = os.path.join(root, fn)
            rel = os.path.relpath(sp, src_dir)
            dp = os.path.join(out_dir, os.path.splitext(rel)[0] + ".js")
            transpile_file(sp, dp)
            count += 1
    return count


def main():
    if len(sys.argv) < 3:
        print("usage: xaml2react.py <src.xaml|dir> <out.js|out_dir>", file=sys.stderr)
        return 1
    src, dst = sys.argv[1], sys.argv[2]
    if os.path.isdir(src):
        n = transpile_tree(src, dst)
        print(f"transpiled {n} files -> {dst}")
        return 0
    transpile_file(src, dst)
    print(dst)
    return 0


if __name__ == "__main__":
    sys.exit(main())
