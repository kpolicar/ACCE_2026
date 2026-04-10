#!/usr/bin/env python3
"""
Generate performance bar charts for the flood simulation report.

Each input gets a subplot. Each bar is one optimization step (sequential, v1, v2, ...).
Bars are stacked to show time breakdown by operation category.
"""

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

# ---------------------------------------------------------------------------
# Data: add new optimization steps by appending to `versions` and each input's
# `times` list.  Each entry in `times` is a dict of category -> seconds.
# Categories not present in a version are assumed 0.
# ---------------------------------------------------------------------------

INPUTS = [
    "debug",
    "small_mountains",
    "custom_clouds",
    "medium_lower_dam",
    "medium_higher_dam",
    "large_mountains",
]

GRID_LABELS = {
    "debug": "90x90",
    "small_mountains": "100x100",
    "custom_clouds": "120x120",
    "medium_lower_dam": "400x300",
    "medium_higher_dam": "256x256",
    "large_mountains": "3072x2048",
}

# Version labels (x-axis groups) -- extend this list with each new optimization
VERSIONS = ["Sequential", "v1: GPU spillage"]

# Categories that make up each bar (bottom to top)
CATEGORIES = ["Kernel", "Memcpy", "Other/CPU"]
COLORS = ["#2196F3", "#FF9800", "#9E9E9E"]

# Per-input, per-version breakdown (seconds).
# Sequential has no GPU breakdown so everything goes under "Other/CPU".
# For CUDA versions we split into Kernel / Memcpy / Other based on nvprof.
DATA = {
    "debug": [
        {"Other/CPU": 0.085},
        {"Kernel": 0.029 * 0.54, "Memcpy": 0.029 * 0.41, "Other/CPU": 0.029 * 0.05},
    ],
    "small_mountains": [
        {"Other/CPU": 0.045},
        {"Kernel": 0.018 * 0.50, "Memcpy": 0.018 * 0.45, "Other/CPU": 0.018 * 0.05},
    ],
    "custom_clouds": [
        {"Other/CPU": 0.342},
        {"Kernel": 0.166 * 0.49, "Memcpy": 0.166 * 0.47, "Other/CPU": 0.166 * 0.04},
    ],
    "medium_lower_dam": [
        {"Other/CPU": 0.976},
        {"Kernel": 0.169 * 0.39, "Memcpy": 0.169 * 0.60, "Other/CPU": 0.169 * 0.01},
    ],
    "medium_higher_dam": [
        {"Other/CPU": 0.995},
        {"Kernel": 0.139 * 0.37, "Memcpy": 0.139 * 0.62, "Other/CPU": 0.139 * 0.01},
    ],
    "large_mountains": [
        {"Other/CPU": 181.359},
        {"Kernel": 15.196 * 0.26, "Memcpy": 15.196 * 0.74, "Other/CPU": 15.196 * 0.00},
    ],
}

# ---------------------------------------------------------------------------
# Plotting
# ---------------------------------------------------------------------------

fig, axes = plt.subplots(2, 3, figsize=(14, 8))
axes = axes.flatten()

bar_width = 0.5

for ax_idx, inp in enumerate(INPUTS):
    ax = axes[ax_idx]
    versions = VERSIONS
    n = len(versions)
    x = np.arange(n)

    bottoms = np.zeros(n)
    for cat_idx, cat in enumerate(CATEGORIES):
        vals = np.array([DATA[inp][v].get(cat, 0.0) for v in range(n)])
        ax.bar(x, vals, bar_width, bottom=bottoms, label=cat, color=COLORS[cat_idx])
        bottoms += vals

    # Annotate total time on top of each bar
    for i in range(n):
        total = sum(DATA[inp][i].values())
        ax.text(i, bottoms[i] + bottoms.max() * 0.02, f"{total:.3f}s",
                ha="center", va="bottom", fontsize=8, fontweight="bold")

    ax.set_title(f"{inp}\n({GRID_LABELS[inp]})", fontsize=10)
    ax.set_xticks(x)
    ax.set_xticklabels(versions, fontsize=8, rotation=15, ha="right")
    ax.set_ylabel("Time (s)")
    ax.set_ylim(0, bottoms.max() * 1.15)

# Shared legend
handles, labels = axes[0].get_legend_handles_labels()
fig.legend(handles, labels, loc="upper center", ncol=len(CATEGORIES),
           fontsize=10, bbox_to_anchor=(0.5, 1.0))

fig.suptitle("Flood Simulation: Time Breakdown per Optimization Step", y=1.03, fontsize=13)
fig.tight_layout()
fig.savefig("perf_breakdown.pdf", bbox_inches="tight", dpi=150)
fig.savefig("perf_breakdown.png", bbox_inches="tight", dpi=150)
print("Saved perf_breakdown.pdf and perf_breakdown.png")
