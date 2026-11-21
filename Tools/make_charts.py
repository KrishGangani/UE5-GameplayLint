"""Draw the README charts (SVG, no dependencies) from existing scan data.

Usage: python -I Tools/make_charts.py
  Reads the S3 baseline batch (stock run, F0) and the S7 batch (F1, all six rules), N = 1,000, seed 1.
  Writes Benchmarks/Charts/detection_by_class.svg and rule_time_per_asset.svg.
"""
from __future__ import annotations

import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RAW = os.path.join(ROOT, "Benchmarks", "Raw")
SUMMARIES = os.path.join(ROOT, "Benchmarks", "Summaries")
OUT = os.path.join(ROOT, "Benchmarks", "Charts")
BASELINE = "2026-11-21_041526_baseline"
FULL = "2026-11-21_083618_s7"

INK, MUTED, GRID, BG = "#1f2328", "#59636e", "#d1d9e0", "#ffffff"
STOCK, LINT = "#9a6700", "#0969da"
FONT = "font-family='-apple-system, Segoe UI, Helvetica, Arial, sans-serif'"


def load(path: str) -> dict:
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


def stock_flagged_by_rule() -> dict[str, list[int]]:
    """Asset-level: a seeded defect counts as flagged if the stock-equivalent run gave its asset any error or warning."""
    truth = load(os.path.join(RAW, f"{BASELINE}_groundtruth.json"))
    report = load(os.path.join(RAW, f"{BASELINE}_F0_N1000_r1.json"))
    flagged = {a["asset"] for a in report["assets"] if a["errors"] or a["warnings"]}
    out: dict[str, list[int]] = {}
    for entry in truth["entries"]:
        if entry["category"] == "defect":
            cell = out.setdefault(entry["rule"], [0, 0])
            cell[0] += entry["asset"] in flagged
            cell[1] += 1
    return out


def text(x, y, value, size=12, color=INK, anchor="start", weight="normal") -> str:
    return f"<text x='{x}' y='{y}' {FONT} font-size='{size}' fill='{color}' text-anchor='{anchor}' font-weight='{weight}'>{value}</text>"


def detection_chart() -> str:
    stock = stock_flagged_by_rule()
    lint = load(os.path.join(SUMMARIES, f"{FULL}_score.json"))["F1"]["rules"]
    rules = sorted(lint, key=lambda r: (not r.startswith("GL.Data"), r))
    left, width, row, top = 210, 360, 34, 56
    height = top + row * len(rules) + 46
    parts = [f"<svg xmlns='http://www.w3.org/2000/svg' width='{left + width + 90}' height='{height}' viewBox='0 0 {left + width + 90} {height}'>",
             f"<rect width='100%' height='100%' fill='{BG}'/>",
             text(16, 24, "Seeded defects found, by defect class (N = 1,000, seed 1)", 14, weight="600"),
             f"<rect x='16' y='34' width='10' height='10' fill='{STOCK}'/>", text(31, 43, "stock Data Validation run (asset flagged at all)", 11, MUTED),
             f"<rect x='300' y='34' width='10' height='10' fill='{LINT}'/>", text(315, 43, "GameplayLint (finding of the right rule)", 11, MUTED)]
    for tick in (0, 50, 100):
        x = left + width * tick / 100
        parts.append(f"<line x1='{x}' y1='{top - 4}' x2='{x}' y2='{top + row * len(rules)}' stroke='{GRID}'/>")
        parts.append(text(x, top + row * len(rules) + 16, f"{tick} %", 11, MUTED, "middle"))
    for i, rule in enumerate(rules):
        y = top + i * row
        found_lint, seeded = lint[rule]
        found_stock = stock.get(rule, [0, seeded])[0]
        parts.append(text(left - 8, y + 17, f"{rule} ({seeded})", 11, INK, "end"))
        for j, (found, color) in enumerate(((found_stock, STOCK), (found_lint, LINT))):
            w = width * found / seeded
            parts.append(f"<rect x='{left}' y='{y + 4 + j * 13}' width='{max(w, 1.5):.1f}' height='11' fill='{color}'/>")
            parts.append(text(left + max(w, 1.5) + 5, y + 14 + j * 13, f"{found}/{seeded}", 10, MUTED))
    parts.append(text(16, height - 10, "Controls (672 clean + 130 near-miss assets): 0 GameplayLint findings. Stock flags tags only through a load warning.", 10, MUTED))
    parts.append("</svg>")
    return "\n".join(parts)


def time_chart() -> str:
    times = load(os.path.join(SUMMARIES, f"{FULL}_score.json"))["F1"]["check_times"]
    rules = sorted(times, key=lambda r: -times[r]["p99_ms"])
    left, width, row, top = 210, 360, 30, 56
    scale = max(t["p99_ms"] for t in times.values()) * 1.15
    height = top + row * len(rules) + 46
    parts = [f"<svg xmlns='http://www.w3.org/2000/svg' width='{left + width + 110}' height='{height}' viewBox='0 0 {left + width + 110} {height}'>",
             f"<rect width='100%' height='100%' fill='{BG}'/>",
             text(16, 24, "Rule time per checked asset, milliseconds (N = 1,000, one F1 run)", 14, weight="600"),
             f"<rect x='16' y='34' width='10' height='10' fill='{LINT}'/>", text(31, 43, "median", 11, MUTED),
             f"<rect x='90' y='34' width='10' height='10' fill='{LINT}' fill-opacity='0.35'/>", text(105, 43, "p99", 11, MUTED),
             text(160, 43, "target SC5: median ≤ 0.5 ms, p99 ≤ 5 ms", 11, MUTED)]
    for i, rule in enumerate(rules):
        y = top + i * row
        t = times[rule]
        parts.append(text(left - 8, y + 15, f"{rule} ({t['assets']})", 11, INK, "end"))
        parts.append(f"<rect x='{left}' y='{y + 4}' width='{width * t['p99_ms'] / scale:.1f}' height='14' fill='{LINT}' fill-opacity='0.35'/>")
        parts.append(f"<rect x='{left}' y='{y + 4}' width='{max(width * t['median_ms'] / scale, 1.5):.1f}' height='14' fill='{LINT}'/>")
        parts.append(text(left + width * t["p99_ms"] / scale + 5, y + 15, f"{t['median_ms']:.4f} / {t['p99_ms']:.4f}", 10, MUTED))
    parts.append(text(16, height - 10, "Numbers in brackets: assets the rule checked. The UpdateBudget rules check only actor and component Blueprints.", 10, MUTED))
    parts.append("</svg>")
    return "\n".join(parts)


def main() -> int:
    os.makedirs(OUT, exist_ok=True)
    for name, svg in (("detection_by_class.svg", detection_chart()), ("rule_time_per_asset.svg", time_chart())):
        with open(os.path.join(OUT, name), "w", encoding="utf-8", newline="\n") as handle:
            handle.write(svg + "\n")
        print("wrote", os.path.join("Benchmarks", "Charts", name))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
