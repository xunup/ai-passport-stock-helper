#!/usr/bin/env python3
"""Verify the embedded Chinese fonts cover every UI glyph and stay in sync."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FONT_DIR = ROOT / "assets" / "fonts"
FONTS = (FONT_DIR / "lv_font_cn_16.c", FONT_DIR / "lv_font_cn_22.c")

# Keep in sync with COMMON_SYMBOLS in tools/gen_cjk_font.py.
SYMBOLS = "，。！？：；、‘’“”（）【】《》…—·￥↑↓"

UI_SOURCES = (
    ROOT / "main" / "ui_app.c",
    ROOT / "main" / "app_controller.c",
    ROOT / "main" / "stock_codes.c",
    ROOT / "main" / "wifi_app.c",
    ROOT / "main" / "web_server.c",
    ROOT / "main" / "quote_worker.c",
    ROOT / "main" / "quote_format.c",
)

CMAP_RE = re.compile(
    r"\.range_start\s*=\s*(\d+),\s*\.range_length\s*=\s*(\d+),"
    r"\s*\.glyph_id_start\s*=\s*\d+,\s*"
    r"\.unicode_list\s*=\s*(\w+|NULL),\s*"
    r"\.glyph_id_ofs_list\s*=\s*\w+,\s*"
    r"\.list_length\s*=\s*(\d+),",
    re.S,
)
LIST_RE = re.compile(r"static const uint16_t (unicode_list_cjk_\d+)\[\] = \{(.*?)\};", re.S)
HEX_RE = re.compile(r"0x([0-9A-Fa-f]{4})")
C_LITERAL_RE = re.compile(r'"(?:\\.|[^"\\])*"')


def gb2312_level_one() -> set[str]:
    chars: set[str] = set()
    for hi in range(0xB0, 0xD8):
        for lo in range(0xA1, 0xFF):
            try:
                ch = bytes((hi, lo)).decode("gb2312")
            except UnicodeDecodeError:
                continue
            if len(ch) == 1 and 0x4E00 <= ord(ch) <= 0x9FA5:
                chars.add(ch)
    return chars


def font_coverage(path: Path) -> tuple[set[int], int]:
    source = path.read_text(encoding="utf-8")
    lists = {name: [int(value, 16) for value in HEX_RE.findall(body)]
             for name, body in LIST_RE.findall(source)}
    covered: set[int] = set()
    sparse_total = 0
    for start, length, list_name, list_length in CMAP_RE.findall(source):
        start = int(start)
        length = int(length)
        list_length = int(list_length)
        if list_name == "NULL":
            covered.update(range(start, start + length))
            continue
        if list_name not in lists:
            raise AssertionError(f"{path.name}: missing {list_name}")
        offsets = lists[list_name]
        if len(offsets) != list_length:
            raise AssertionError(
                f"{path.name}: {list_name} has {len(offsets)} entries, cmap says {list_length}")
        sparse_total += len(offsets)
        covered.update(start + offset for offset in offsets)
    return covered, sparse_total


def ui_glyphs() -> set[str]:
    glyphs: set[str] = set()
    for path in UI_SOURCES:
        source = path.read_text(encoding="utf-8")
        for literal in C_LITERAL_RE.findall(source):
            glyphs.update(ch for ch in literal if ord(ch) > 0x7F)
    return glyphs


def main() -> None:
    required = {chr(c) for c in range(0x20, 0x7F)} | gb2312_level_one() | set(SYMBOLS)
    ui_chars = ui_glyphs()

    coverages = {}
    for path in FONTS:
        covered, sparse_total = font_coverage(path)
        coverages[path.name] = covered
        covered_chars = {chr(codepoint) for codepoint in covered}
        assert sparse_total == 3778, f"{path.name}: sparse glyphs={sparse_total}, expected 3778"
        missing = required - covered_chars
        assert not missing, (
            f"{path.name}: missing {len(missing)} required glyphs: "
            + " ".join(f"U+{ord(ch):04X}({ch})" for ch in sorted(missing)[:12]))
        missing_ui = ui_chars - covered_chars
        assert not missing_ui, (
            f"{path.name}: UI text uses uncovered glyphs: "
            + " ".join(f"U+{ord(ch):04X}({ch})" for ch in sorted(missing_ui)))
        assert "@" in covered_chars
        print(f"{path.name}: {len(covered)} codepoints, {sparse_total} CJK glyphs, UI text covered")

    first, second = (coverages[name] for name in (FONTS[0].name, FONTS[1].name))
    assert first == second, "16px and 22px font coverage differ"

    print(f"UI glyphs checked: {len(ui_chars)}; font coverage tests: PASS")


if __name__ == "__main__":
    main()
