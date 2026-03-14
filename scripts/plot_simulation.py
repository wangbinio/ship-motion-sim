#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator


def read_state_csv(path: Path) -> dict[str, list[float]]:
    columns = {
        "time_s": [],
        "lat_deg": [],
        "lon_deg": [],
        "heading_deg": [],
        "speed_mps": [],
        "yaw_rate_deg_s": [],
    }

    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        expected = list(columns.keys())
        if reader.fieldnames != expected:
            raise ValueError(f"unexpected CSV header: {reader.fieldnames}, expected: {expected}")
        for row in reader:
            for key in columns:
                columns[key].append(float(row[key]))

    if not columns["time_s"]:
        raise ValueError(f"no simulation rows found in {path}")
    return columns


def read_command_csv(path: Path | None) -> list[tuple[float, str, str]]:
    if path is None:
        return []

    commands: list[tuple[float, str, str]] = []
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames != ["time_s", "type", "value"]:
            raise ValueError(f"unexpected command CSV header: {reader.fieldnames}")
        for row in reader:
            commands.append((float(row["time_s"]), row["type"], row["value"]))
    return commands


def compute_padded_bounds(values: list[float], min_padding: float) -> tuple[float, float]:
    lower = min(values)
    upper = max(values)
    span = upper - lower
    padding = max(span * 0.12, min_padding)
    return lower - padding, upper + padding


def find_state_indices_for_commands(
    state: dict[str, list[float]],
    commands: list[tuple[float, str, str]],
) -> list[int]:
    indices: list[int] = []
    times = state["time_s"]
    cursor = 0
    for command_time, _, _ in commands:
        while cursor + 1 < len(times) and times[cursor] < command_time:
            cursor += 1
        indices.append(cursor)
    return indices


def plot_simple_map(
    axis,
    state: dict[str, list[float]],
    commands: list[tuple[float, str, str]],
) -> None:
    lon_values = state["lon_deg"]
    lat_values = state["lat_deg"]
    lon_min, lon_max = compute_padded_bounds(lon_values, 0.0005)
    lat_min, lat_max = compute_padded_bounds(lat_values, 0.0005)

    axis.set_facecolor("#dbeafe")
    axis.set_xlim(lon_min, lon_max)
    axis.set_ylim(lat_min, lat_max)
    axis.set_title("Trajectory on Simple Map")
    axis.set_xlabel("Longitude (deg)")
    axis.set_ylabel("Latitude (deg)")
    axis.grid(True, linestyle="--", linewidth=0.8, color="#93c5fd", alpha=0.65)
    axis.xaxis.set_major_locator(MaxNLocator(nbins=6))
    axis.yaxis.set_major_locator(MaxNLocator(nbins=6))
    axis.set_aspect("equal", adjustable="box")

    axis.plot(lon_values, lat_values, color="#0f766e", linewidth=2.4, zorder=3)
    axis.scatter(lon_values[0], lat_values[0], color="#16a34a", s=70, marker="o", label="Start", zorder=4)
    axis.scatter(lon_values[-1], lat_values[-1], color="#dc2626", s=85, marker="X", label="End", zorder=4)

    arrow_step = max(1, len(lon_values) // 18)
    for index in range(0, len(lon_values) - arrow_step, arrow_step):
        axis.annotate(
            "",
            xy=(lon_values[index + arrow_step], lat_values[index + arrow_step]),
            xytext=(lon_values[index], lat_values[index]),
            arrowprops={"arrowstyle": "->", "color": "#0f766e", "lw": 1.0, "alpha": 0.55},
            zorder=3,
        )

    if commands:
        command_indices = find_state_indices_for_commands(state, commands)
        for index, (command_time, command_type, command_value) in zip(command_indices, commands):
            color = "#2563eb" if command_type == "engine" else "#ea580c"
            axis.scatter(lon_values[index], lat_values[index], color=color, s=30, zorder=5)
            axis.annotate(
                f"{command_time:.0f}s\n{command_type}={command_value}",
                (lon_values[index], lat_values[index]),
                textcoords="offset points",
                xytext=(6, 6),
                fontsize=7,
                color="#111827",
                bbox={"boxstyle": "round,pad=0.2", "fc": "white", "ec": color, "alpha": 0.8},
                zorder=6,
            )

    axis.legend(loc="best")


def plot_results(
    state: dict[str, list[float]],
    commands: list[tuple[float, str, str]],
    output_path: Path,
    title: str,
) -> None:
    times = state["time_s"]

    fig, axes = plt.subplots(5, 1, figsize=(14, 16), constrained_layout=True)
    fig.suptitle(title, fontsize=16)

    plot_simple_map(axes[0], state, commands)

    axes[1].plot(times, state["heading_deg"], color="#b45309", linewidth=1.8)
    axes[1].set_title("Heading")
    axes[1].set_ylabel("deg")
    axes[1].grid(True, linestyle="--", alpha=0.35)

    axes[2].plot(times, state["speed_mps"], color="#1d4ed8", linewidth=1.8)
    axes[2].set_title("Speed")
    axes[2].set_ylabel("m/s")
    axes[2].grid(True, linestyle="--", alpha=0.35)

    axes[3].plot(times, state["yaw_rate_deg_s"], color="#dc2626", linewidth=1.8)
    axes[3].set_title("Yaw Rate")
    axes[3].set_ylabel("deg/s")
    axes[3].grid(True, linestyle="--", alpha=0.35)

    if commands:
        command_times = [item[0] for item in commands]
        command_labels = [f"{item[1]}={item[2]}" for item in commands]
        y_values = []
        for index, _ in enumerate(commands):
            y_values.append(index % 3)

        axes[4].scatter(command_times, y_values, color="#7c3aed", s=50)
        for command_time, label, y_value in zip(command_times, command_labels, y_values):
            axes[4].annotate(
                label,
                (command_time, y_value),
                textcoords="offset points",
                xytext=(0, 8),
                ha="center",
                fontsize=8,
                rotation=20,
            )
        axes[4].set_yticks([0, 1, 2])
        axes[4].set_yticklabels(["level0", "level1", "level2"])
        axes[4].set_title("Command Timeline")
        axes[4].set_ylabel("stagger")
        axes[4].grid(True, linestyle="--", alpha=0.35)
    else:
        axes[4].axis("off")

    for axis in axes[1:4]:
        for command_time, command_type, _ in commands:
            color = "#2563eb" if command_type == "engine" else "#ea580c"
            axis.axvline(command_time, color=color, linestyle=":", alpha=0.25)

    axes[4].set_xlabel("Time (s)")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, dpi=160)
    plt.close(fig)


def main() -> int:
    parser = argparse.ArgumentParser(description="Plot ship motion simulation CSV results.")
    parser.add_argument("--input", required=True, help="Path to simulation output CSV.")
    parser.add_argument("--output", required=True, help="Path to output image file.")
    parser.add_argument("--commands", help="Optional command CSV for timeline annotations.")
    parser.add_argument("--title", default="Ship Motion Simulation Overview")
    args = parser.parse_args()

    input_path = Path(args.input)
    output_path = Path(args.output)
    commands_path = Path(args.commands) if args.commands else None

    state = read_state_csv(input_path)
    commands = read_command_csv(commands_path)
    plot_results(state, commands, output_path, args.title)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
