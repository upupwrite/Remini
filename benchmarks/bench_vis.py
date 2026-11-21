import json, sys
from pathlib import Path
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

STAGE_ORDER = ["structure-scan", "parse-only", "document-pipeline", "md4c-full-document"]
COLORS = {"structure-scan": "#4E79A7", "parse-only": "#F28E2B",
          "document-pipeline": "#E15759", "md4c-full-document": "#59A14F"}

# English-only labels: no CJK font dependency, renders identically everywhere.
plt.rcParams["font.family"] = "DejaVu Sans"
plt.rcParams["axes.unicode_minus"] = False


def load_bench(path):
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    if not isinstance(data, dict) or "results" not in data:
        raise ValueError("JSON top level must be an object containing 'results'")
    if not data["results"]:
        raise ValueError("'results' is empty")
    return data, data["results"]


def stage_key(s):
    return STAGE_ORDER.index(s) if s in STAGE_ORDER else len(STAGE_ORDER)


def sort_stages(stages):
    return sorted(stages, key=stage_key)


def fmt_ms(v):
    if v >= 1000:
        return f"{v/1000:.2f} s"
    if v >= 1:
        return f"{v:.2f} ms"
    return f"{v*1000:.1f} us"


def annotate_bars(ax, bars, values, fmt, fontsize=8):
    for bar, val in zip(bars, values):
        ax.annotate(fmt(val), (bar.get_x() + bar.get_width() / 2, bar.get_height()),
                    ha="center", va="bottom", fontsize=fontsize,
                    xytext=(0, 3), textcoords="offset points")


def plot_benchmark(data, records, out_png="bench.png", show=False):
    stages = sort_stages({r["stage"] for r in records})
    by = {s: [r for r in records if r["stage"] == s] for s in stages}
    agg = lambda k: np.array([np.mean([r[k] for r in by[s]]) for s in stages])
    mean = agg("mean_ms"); lo, hi = agg("min_ms"), agg("max_ms")
    mib = agg("mib_per_second"); npc = agg("nanoseconds_per_character")
    lps = agg("lines_per_second")
    colors = [COLORS.get(s, "#888888") for s in stages]
    x = np.arange(len(stages))

    fig = plt.figure(figsize=(13, 9), constrained_layout=True)
    fig.suptitle(f"{data.get('benchmark', 'benchmark')}  |  {data.get('compiler', '')}",
                 fontsize=13, fontweight="bold")
    gs = fig.add_gridspec(2, 2, hspace=0.35, wspace=0.25)

    # (a) mean time with min/max error bars
    ax1 = fig.add_subplot(gs[0, 0])
    bars = ax1.bar(x, mean, width=0.55, color=colors, yerr=[mean - lo, hi - mean],
                   capsize=4, error_kw=dict(ecolor="#444", lw=1.2))
    ax1.set_yscale("log")
    ax1.set_xticks(x, stages, rotation=20, ha="right", fontsize=9)
    ax1.set_ylabel("mean time (ms, log)")
    ax1.grid(axis="y", ls="--", alpha=0.4)
    ax1.set_title("(a) Mean time per stage (error bars: min/max)", fontsize=11)
    annotate_bars(ax1, bars, mean, fmt_ms)

    # (b) throughput
    ax2 = fig.add_subplot(gs[0, 1])
    bars = ax2.barh(stages, mib, color=colors)
    ax2.set_xscale("log")
    ax2.set_xlabel("throughput (MiB/s, log)")
    ax2.set_title("(b) Throughput comparison", fontsize=11)
    ax2.invert_yaxis()
    ax2.grid(axis="x", ls="--", alpha=0.4)
    for bar, val in zip(bars, mib):
        ax2.annotate(f"{val:.2f}", (bar.get_width(), bar.get_y() + bar.get_height() / 2),
                     ha="left", va="center", fontsize=8,
                     xytext=(4, 0), textcoords="offset points")

    # (c) min/mean/median/max grouped
    ax3 = fig.add_subplot(gs[1, 0])
    width = 0.2
    for i, (k, c) in enumerate(zip(["min_ms", "mean_ms", "median_ms", "max_ms"],
                                    ["#9ecae1", "#4E79A7", "#2171b5", "#08519c"])):
        ax3.bar(x + (i - 1.5) * width, agg(k), width, label=k, color=c)
    ax3.set_yscale("log")
    ax3.set_xticks(x, stages, rotation=20, ha="right", fontsize=9)
    ax3.set_ylabel("time (ms, log)")
    ax3.legend(fontsize=8, ncol=2)
    ax3.grid(axis="y", ls="--", alpha=0.4)
    ax3.set_title("(c) Distribution: min / mean / median / max", fontsize=11)

    # (d) per-character cost + lines per second (twin axis)
    ax4 = fig.add_subplot(gs[1, 1])
    b1 = ax4.bar(x - 0.18, npc, width=0.36, color="#F28E2B", label="ns / char")
    ax4.set_yscale("log")
    ax4.set_xticks(x, stages, rotation=20, ha="right", fontsize=9)
    ax4.set_ylabel("ns per character (log)")
    ax4.grid(axis="y", ls="--", alpha=0.4)
    annotate_bars(ax4, b1, npc, lambda v: f"{v:,.1f}")
    ax4b = ax4.twinx()
    ax4b.plot(x, lps, "o-", color="#59A14F", lw=1.5, label="lines / s")
    ax4b.set_yscale("log")
    ax4b.set_ylabel("lines per second (log)")
    ax4b.tick_params(axis="y", labelcolor="#59A14F")
    ax4.set_title("(d) Per-character cost and lines per second", fontsize=11)
    h1, l1 = ax4.get_legend_handles_labels()
    h2, l2 = ax4b.get_legend_handles_labels()
    ax4.legend(h1 + h2, l1 + l2, fontsize=8, loc="upper left")

    fig.text(0.5, 0.015,
             f"profile={data.get('profile')}  iterations={data.get('iterations')}  "
             f"warmup={data.get('warmup')}  md4c={data.get('md4c_version')}  "
             f"qt={data.get('qt_version')}  dialect={data.get('md4c_dialect')}",
             ha="center", fontsize=8, color="#555")
    fig.savefig(out_png, dpi=150)
    if show:
        plt.show()
    plt.close(fig)
    return out_png


def print_summary(records):
    print(f"{'stage':<22}{'mean_ms':>9}{'min_ms':>9}{'max_ms':>9}{'MiB/s':>9}{'ns/char':>10}{'lines/s':>12}")
    for s in sort_stages({r["stage"] for r in records}):
        rs = [r for r in records if r["stage"] == s]
        m = lambda k: np.mean([r[k] for r in rs])
        print(f"{s:<22}{m('mean_ms'):>9.3f}{m('min_ms'):>9.3f}{m('max_ms'):>9.3f}"
              f"{m('mib_per_second'):>9.2f}{m('nanoseconds_per_character'):>10.1f}"
              f"{m('lines_per_second'):>12.0f}")


if __name__ == "__main__":
    path = sys.argv[1] if len(sys.argv) > 1 else "bench.json"
    out = sys.argv[2] if len(sys.argv) > 2 else "bench.png"
    data, records = load_bench(path)
    print_summary(records)
    print("saved ->", plot_benchmark(data, records, out_png=out))
