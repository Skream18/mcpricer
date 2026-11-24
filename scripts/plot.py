"""Plot benchmark results. Usage: python scripts/plot.py [results directory]

Reads scaling.csv and convergence.csv and writes speedup.png and convergence.png next to them.
Requires matplotlib.
"""

import csv
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

SURFACE, INK, MUTED, GRID = "#fcfcfb", "#0b0b0b", "#52514e", "#e4e3df"
# Checked for colour-vision deficiency; markers differ too, so colour is never the only cue.
STYLES = [("#2a78d6", "o"), ("#eb6834", "s"), ("#1baf7a", "^"), ("#4a3aa7", "D")]


def read(path):
    with path.open(newline="", encoding="utf-8") as file:
        return list(csv.DictReader(file))


def new_axes(title, xlabel, ylabel):
    figure, axes = plt.subplots(figsize=(8, 5), facecolor=SURFACE)
    axes.set_facecolor(SURFACE)
    axes.set_title(title, loc="left", color=INK, fontsize=12)
    axes.set_xlabel(xlabel, color=MUTED)
    axes.set_ylabel(ylabel, color=MUTED)
    axes.grid(color=GRID, linewidth=0.8)
    axes.set_axisbelow(True)
    axes.tick_params(colors=MUTED, which="both")
    for name, spine in axes.spines.items():
        spine.set_visible(name in ("left", "bottom"))
        spine.set_color(GRID)
    return figure, axes


def draw(axes, points, label, style):
    colour, marker = style
    xs, ys = zip(*sorted(points))
    axes.plot(xs, ys, color=colour, marker=marker, markersize=8, linewidth=2, label=label,
              markeredgecolor=SURFACE, markeredgewidth=1.5)


def save(figure, axes, path):
    axes.legend(frameon=False, labelcolor=INK)
    figure.tight_layout()
    figure.savefig(path, dpi=150)
    plt.close(figure)


def plot_speedup(rows, path):
    figure, axes = new_axes("Speedup over the serial backend", "Threads", "Speedup")
    rows = [row for row in rows if row["backend"] == "threaded"]
    series = [("asian-arith", "1000000"), ("asian-arith", "100000"), ("asian-arith", "10000"),
              ("euro-call", "10000000")]
    for (option, paths), style in zip(series, STYLES):
        points = [(int(r["threads"]), float(r["speedup"])) for r in rows
                  if (r["option"], r["paths"]) == (option, paths)]
        if points:
            draw(axes, points, f"{option}, {int(paths):,} paths", style)
    threads = sorted({int(row["threads"]) for row in rows})
    axes.plot(threads, threads, color=MUTED, linewidth=1, linestyle="--", label="ideal")
    axes.set_xticks(threads)
    axes.set_ylim(bottom=0)
    save(figure, axes, path)


def plot_convergence(rows, path):
    figure, axes = new_axes("Arithmetic Asian call: standard error by method", "Simulated paths",
                            "Standard error")
    rows = [row for row in rows if row["option"] == "asian-arith"]
    methods = [("plain", "plain"), ("antithetic", "antithetic"), ("control", "control variate"),
               ("both", "both")]
    for (method, label), style in zip(methods, STYLES):
        draw(axes, [(int(r["paths"]), float(r["std_error"])) for r in rows
                    if r["method"] == method], label, style)
    plain = sorted((int(r["paths"]), float(r["std_error"])) for r in rows if r["method"] == "plain")
    n0, error0 = plain[0]
    xs = [n for n, _ in plain]
    axes.plot(xs, [0.5 * error0 * (n0 / n) ** 0.5 for n in xs], color=MUTED, linewidth=1,
              linestyle="--", label="slope of $1/\\sqrt{N}$")
    axes.set_xscale("log")
    axes.set_yscale("log")
    save(figure, axes, path)


def main():
    directory = Path(sys.argv[1] if len(sys.argv) > 1 else "bench/results")
    plot_speedup(read(directory / "scaling.csv"), directory / "speedup.png")
    plot_convergence(read(directory / "convergence.csv"), directory / "convergence.png")


if __name__ == "__main__":
    main()
