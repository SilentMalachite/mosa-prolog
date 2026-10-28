#!/usr/bin/env python3
"""Unicode 18.0.0 の UCD から src/unicode_tables.inc を生成する。

ビルドからは呼ばない。失敗したときは終了コード 1 で、既存の表を残す。
ASCII の文字種は ISO 6.5 の固定表で決めるため、カテゴリ区間は U+0080 からにする。
"""

from __future__ import annotations

import argparse
import os
import sys
import urllib.error
import urllib.request
from pathlib import Path

UNICODE_VERSION = "18.0.0"
BASE_URL = f"https://www.unicode.org/Public/{UNICODE_VERSION}/ucd/"
ROOT = Path(__file__).resolve().parent.parent
OUT_PATH = ROOT / "src" / "unicode_tables.inc"
VERSION_PATH = ROOT / "docs" / "unicode-version.md"
MAX_CODE_POINT = 0x10FFFF

# 表が壊れたまま置き換えないための定点。カテゴリは UnicodeData の値。
CATEGORY_CHECKS = {
    0x0085: "Cc",
    0x00C4: "Lu",
    0x0301: "Mn",
    0x3000: "Zs",
    0x3042: "Lo",
    0x4E00: "Lo",
    0x89AA: "Lo",
    0xFEFF: "Cf",
    0xFF08: "Ps",
    0xFF10: "Nd",
    0xFF3F: "Pc",
    0x20BB7: "Lo",
}
WIDE_PRESENT = (0x3000, 0x4E00, 0xFF08, 0xFF10, 0x20BB7)
WIDE_ABSENT = (0x0041, 0x007F)
# 単純大文字写像。写像の無い符号位置は自分自身。
UPPER_CHECKS = {
    0x0061: 0x0041,
    0x00DF: 0x00DF,
    0x00E9: 0x00C9,
    0x03B1: 0x0391,
    0xFF41: 0xFF21,
}


def fetch(url: str) -> str:
    request = urllib.request.Request(
        url, headers={"User-Agent": "mosa-prolog-gen-unicode-tables"}
    )
    try:
        with urllib.request.urlopen(request, timeout=120) as response:
            payload = response.read()
    except urllib.error.URLError as exc:
        raise RuntimeError(f"取得に失敗しました: {url}: {exc}") from exc
    return payload.decode("utf-8-sig")


def load_texts(unicode_dir: str | None) -> tuple[str, str]:
    names = ("UnicodeData.txt", "EastAsianWidth.txt")
    if unicode_dir is None:
        return tuple(fetch(BASE_URL + name) for name in names)
    directory = Path(unicode_dir)
    texts: list[str] = []
    for name in names:
        path = directory / name
        if not path.is_file():
            raise RuntimeError(f"ファイルがありません: {path}")
        texts.append(path.read_text(encoding="utf-8-sig"))
    return texts[0], texts[1]


def parse_unicode_data(text: str) -> list[tuple[int, int, str]]:
    rows: list[tuple[int, str, str]] = []
    for line_number, raw in enumerate(text.splitlines(), start=1):
        if not raw or raw.startswith("#"):
            continue
        parts = raw.split(";")
        if len(parts) < 3:
            raise RuntimeError(f"UnicodeData.txt:{line_number} の列が足りません")
        try:
            code_point = int(parts[0], 16)
        except ValueError as exc:
            raise RuntimeError(f"UnicodeData.txt:{line_number} の符号位置が不正です") from exc
        category = parts[2]
        if len(category) != 2 or not category.isascii() or not category.isalpha():
            raise RuntimeError(f"UnicodeData.txt:{line_number} のカテゴリが不正です")
        rows.append((code_point, parts[1], category))

    ranges: list[tuple[int, int, str]] = []
    index = 0
    while index < len(rows):
        code_point, name, category = rows[index]
        if name.endswith(", First>"):
            if index + 1 >= len(rows):
                raise RuntimeError(f"U+{code_point:04X} の First に対応する Last がありません")
            last_cp, last_name, last_category = rows[index + 1]
            if (
                not last_name.endswith(", Last>")
                or last_category != category
                or last_cp < code_point
            ):
                raise RuntimeError(f"U+{code_point:04X} の First/Last が対になっていません")
            ranges.append((code_point, last_cp, category))
            index += 2
            continue
        if name.endswith(", Last>"):
            raise RuntimeError(f"U+{code_point:04X} の Last に First がありません")
        ranges.append((code_point, code_point, category))
        index += 1
    return ranges


def parse_east_asian_width(text: str) -> list[tuple[int, int]]:
    ranges: list[tuple[int, int]] = []
    for line_number, raw in enumerate(text.splitlines(), start=1):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        code_field, separator, width_field = line.partition(";")
        if separator != ";":
            raise RuntimeError(f"EastAsianWidth.txt:{line_number} に ';' がありません")
        code_field = code_field.strip()
        width = width_field.strip().split()[0] if width_field.strip() else ""
        if not width.isascii() or not width.isalpha():
            raise RuntimeError(f"EastAsianWidth.txt:{line_number} の幅が不正です")
        if width not in ("W", "F"):
            continue
        try:
            if ".." in code_field:
                start_text, end_text = code_field.split("..", 1)
                first = int(start_text, 16)
                last = int(end_text, 16)
            else:
                first = last = int(code_field, 16)
        except ValueError as exc:
            raise RuntimeError(f"EastAsianWidth.txt:{line_number} の符号位置が不正です") from exc
        if last < first:
            raise RuntimeError(f"EastAsianWidth.txt:{line_number} の区間が逆です")
        ranges.append((first, last))
    return ranges


def merge_categories(ranges: list[tuple[int, int, str]]) -> list[tuple[int, int, str]]:
    merged: list[tuple[int, int, str]] = []
    for first, last, category in sorted(ranges):
        if first > last or last > MAX_CODE_POINT:
            raise RuntimeError(f"U+{first:04X}..U+{last:04X} は符号位置の範囲外です")
        if not merged:
            merged.append((first, last, category))
            continue
        prev_first, prev_last, prev_category = merged[-1]
        if first <= prev_last and category != prev_category:
            raise RuntimeError(
                f"U+{prev_first:04X}..U+{prev_last:04X} と U+{first:04X} のカテゴリが重なります"
            )
        if first <= prev_last + 1 and category == prev_category:
            merged[-1] = (prev_first, max(prev_last, last), category)
            continue
        merged.append((first, last, category))
    return merged


def merge_spans(ranges: list[tuple[int, int]]) -> list[tuple[int, int]]:
    merged: list[tuple[int, int]] = []
    for first, last in sorted(ranges):
        if first > last or last > MAX_CODE_POINT:
            raise RuntimeError(f"U+{first:04X}..U+{last:04X} は符号位置の範囲外です")
        if not merged:
            merged.append((first, last))
            continue
        prev_first, prev_last = merged[-1]
        if first <= prev_last + 1:
            merged[-1] = (prev_first, max(prev_last, last))
            continue
        merged.append((first, last))
    return merged


def drop_ascii(ranges: list[tuple[int, int, str]]) -> list[tuple[int, int, str]]:
    # 分類側は U+007F 以下を固定表で決める。表に残すと境界をまたぐ区間ができる。
    return [(max(first, 0x80), last, category) for first, last, category in ranges if last >= 0x80]


def contains(ranges: list[tuple[int, int]] | list[tuple[int, int, str]], code_point: int) -> int | None:
    low = 0
    high = len(ranges)
    while low < high:
        mid = (low + high) // 2
        if ranges[mid][1] < code_point:
            low = mid + 1
        else:
            high = mid
    if low < len(ranges) and ranges[low][0] <= code_point <= ranges[low][1]:
        return low
    return None


def category_at(ranges: list[tuple[int, int, str]], code_point: int) -> str | None:
    index = contains(ranges, code_point)
    if index is None:
        return None
    return ranges[index][2]


def ensure_categories(ranges: list[tuple[int, int, str]]) -> None:
    previous_last = 0x7F
    previous_category = ""
    covered = 0
    for first, last, category in ranges:
        if first <= previous_last or last > MAX_CODE_POINT or len(category) != 2:
            raise RuntimeError(f"カテゴリ区間が不正です: U+{first:04X}..U+{last:04X}")
        if first == previous_last + 1 and category == previous_category:
            raise RuntimeError(f"隣接する {category} が結合されていません: U+{first:04X}")
        covered += last - first + 1
        previous_last = last
        previous_category = category
    if covered < 100_000:
        raise RuntimeError(f"カテゴリの被覆 {covered} が少なすぎます。First/Last を区間にしていない可能性があります")
    for code_point, expected in CATEGORY_CHECKS.items():
        actual = category_at(ranges, code_point)
        if actual != expected:
            raise RuntimeError(f"U+{code_point:04X} のカテゴリが {actual} です。期待は {expected}")


def parse_simple_uppercase(text: str) -> list[tuple[int, int, int]]:
    # UnicodeData の第13列。空なら単純大文字写像は無い。First/Last は文字ではない。
    points: list[tuple[int, int]] = []
    for line_number, raw in enumerate(text.splitlines(), start=1):
        if not raw or raw.startswith("#"):
            continue
        parts = raw.split(";")
        if len(parts) < 13:
            raise RuntimeError(f"UnicodeData.txt:{line_number} に単純大文字写像の列がありません")
        name = parts[1]
        if name.endswith(", First>") or name.endswith(", Last>"):
            continue
        upper = parts[12].strip()
        if not upper:
            continue
        try:
            code_point = int(parts[0], 16)
            target = int(upper, 16)
        except ValueError as exc:
            raise RuntimeError(f"UnicodeData.txt:{line_number} の単純大文字写像が不正です") from exc
        if target != code_point:
            points.append((code_point, target))
    return merge_upper(points)


def merge_upper(points: list[tuple[int, int]]) -> list[tuple[int, int, int]]:
    # 符号位置が連続し、差が同じ区間だけをまとめる。間の未写像は入れない。
    ranges: list[tuple[int, int, int]] = []
    for code_point, target in sorted(points):
        delta = target - code_point
        if ranges and ranges[-1][1] + 1 == code_point and ranges[-1][2] == delta:
            ranges[-1] = (ranges[-1][0], code_point, delta)
            continue
        ranges.append((code_point, code_point, delta))
    return ranges


def ensure_wide(ranges: list[tuple[int, int]]) -> None:
    previous_last = -1
    for first, last in ranges:
        if first <= previous_last or last > MAX_CODE_POINT:
            raise RuntimeError(f"幅の区間が不正です: U+{first:04X}..U+{last:04X}")
        previous_last = last
    for code_point in WIDE_PRESENT:
        if contains(ranges, code_point) is None:
            raise RuntimeError(f"U+{code_point:04X} が東アジア幅 W/F に入っていません")
    for code_point in WIDE_ABSENT:
        if contains(ranges, code_point) is not None:
            raise RuntimeError(f"U+{code_point:04X} は東アジア幅 W/F ではありません")


def upper_at(ranges: list[tuple[int, int, int]], code_point: int) -> int:
    index = contains(ranges, code_point)
    if index is None:
        return code_point
    return code_point + ranges[index][2]


def ensure_upper(ranges: list[tuple[int, int, int]]) -> None:
    previous_last = -1
    for first, last, delta in ranges:
        if first <= previous_last or last > MAX_CODE_POINT or delta == 0:
            raise RuntimeError(f"単純大文字写像の区間が不正です: U+{first:04X}")
        if first + delta < 0 or last + delta > MAX_CODE_POINT:
            raise RuntimeError(f"単純大文字写像が符号位置の外です: U+{first:04X}")
        previous_last = last
    for code_point, expected in UPPER_CHECKS.items():
        actual = upper_at(ranges, code_point)
        if actual != expected:
            raise RuntimeError(f"U+{code_point:04X} の単純大文字写像が U+{actual:04X} です")


def emit(
    categories: list[tuple[int, int, str]],
    wide: list[tuple[int, int]],
    upper: list[tuple[int, int, int]],
) -> str:
    lines = [
        "// Unicode 18.0.0 の一般カテゴリ区間、東アジア幅が W または F の区間、",
        "// 単純大文字写像の区間。差は符号位置へ足す値。",
        "// tools/gen_unicode_tables.py が生成する。手で編集しない。",
        "// ASCII は ISO 6.5 の固定表で分類するため、カテゴリ区間は U+0080 からにする。",
        f'constexpr std::string_view k_unicode_version{{"{UNICODE_VERSION}"}};',
        "",
        f"constexpr std::array<CategoryRange, {len(categories)}> k_category_ranges{{{{",
    ]
    for first, last, category in categories:
        lines.append(
            f"    CategoryRange{{0x{first:04X}, 0x{last:04X}, '{category[0]}', '{category[1]}'}},"
        )
    lines.append("}};")
    lines.append("")
    lines.append(f"constexpr std::array<WideRange, {len(wide)}> k_wide_ranges{{{{")
    for first, last in wide:
        lines.append(f"    WideRange{{0x{first:04X}, 0x{last:04X}}},")
    lines.append("}};")
    lines.append("")
    lines.append(f"constexpr std::array<UpperRange, {len(upper)}> k_upper_ranges{{{{")
    for first, last, delta in upper:
        lines.append(f"    UpperRange{{0x{first:04X}, 0x{last:04X}, {delta}}},")
    lines.append("}};")
    lines.append("")
    return "\n".join(lines)


def publish(text: str) -> None:
    inc_tmp = OUT_PATH.with_name(OUT_PATH.name + ".tmp")
    version_tmp = VERSION_PATH.with_name(VERSION_PATH.name + ".tmp")
    try:
        inc_tmp.write_text(text, encoding="utf-8", newline="\n")
        version_tmp.write_text(UNICODE_VERSION + "\n", encoding="utf-8", newline="\n")
        os.replace(inc_tmp, OUT_PATH)
        os.replace(version_tmp, VERSION_PATH)
    except Exception:
        for path in (inc_tmp, version_tmp):
            try:
                path.unlink()
            except OSError:
                pass
        raise


def generate(unicode_dir: str | None) -> tuple[int, int, int]:
    data_text, width_text = load_texts(unicode_dir)
    categories = drop_ascii(merge_categories(parse_unicode_data(data_text)))
    wide = merge_spans(parse_east_asian_width(width_text))
    upper = parse_simple_uppercase(data_text)
    ensure_categories(categories)
    ensure_wide(wide)
    ensure_upper(upper)
    publish(emit(categories, wide, upper))
    return len(categories), len(wide), len(upper)


def main() -> int:
    parser = argparse.ArgumentParser(description="Unicode 18.0.0 の文字分類表を生成する")
    parser.add_argument(
        "--unicode-dir",
        help="UnicodeData.txt と EastAsianWidth.txt があるディレクトリ。無いときは UCD を取得する",
    )
    args = parser.parse_args()
    try:
        category_count, wide_count, upper_count = generate(args.unicode_dir)
    except Exception as exc:
        print(f"生成に失敗しました: {exc}", file=sys.stderr)
        return 1
    print(f"categories {category_count}")
    print(f"wide {wide_count}")
    print(f"upper {upper_count}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
