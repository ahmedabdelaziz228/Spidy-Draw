"""
path_optimizer.py - Path cleanup and ordering optimizer.

This module belongs to Muhammad's vectorization stage. It makes extracted paths easier
for Fathy's G-code generator and the motion-control layer to handle.
"""

from __future__ import annotations

from typing import List, Tuple


def distance(a: Tuple[float, float], b: Tuple[float, float]) -> float:
    dx = a[0] - b[0]
    dy = a[1] - b[1]
    return (dx * dx + dy * dy) ** 0.5


def path_endpoints(path) -> Tuple[Tuple[float, float], Tuple[float, float]]:
    return (path.points[0].x, path.points[0].y), (path.points[-1].x, path.points[-1].y)


def centroid_of(path) -> Tuple[float, float]:
    if not path.points:
        return 0.0, 0.0
    xs = [p.x for p in path.points]
    ys = [p.y for p in path.points]
    return sum(xs) / len(xs), sum(ys) / len(ys)


def remove_redundant_points(path, min_distance: float = 0.25):
    """Remove consecutive points that are too close together."""
    if not getattr(path, "points", None):
        return path
    filtered = [path.points[0]]
    for p in path.points[1:]:
        if filtered[-1].distance_to(p) >= min_distance:
            filtered.append(p)
    if getattr(path, "closed", False) and len(filtered) > 2:
        if filtered[0].distance_to(filtered[-1]) < min_distance:
            filtered.pop()
    path.points = filtered
    if hasattr(path, "calculate_length"):
        path.calculate_length()
    return path


def filter_short_paths(paths: List, min_length_mm: float = 1.0) -> List:
    output = []
    for p in paths:
        if hasattr(p, "calculate_length"):
            p.calculate_length()
        if len(getattr(p, "points", [])) >= 2 and getattr(p, "length_mm", 0.0) >= min_length_mm:
            output.append(p)
    return output


def order_paths(paths: List, start_point: Tuple[float, float] = (0.0, 0.0), allow_reverse_open_paths: bool = True) -> List:
    """
    Greedy nearest-neighbor ordering. Open paths may be reversed to reduce travel.
    Closed paths keep their closed flag when reordered/reversed.
    """
    if not paths:
        return []

    remaining = [p for p in paths if getattr(p, "points", None)]
    ordered = []
    current_point = start_point

    while remaining:
        best_idx = 0
        best_distance = float("inf")
        best_reversed = False

        for idx, path in enumerate(remaining):
            start, end = path_endpoints(path)
            d_start = distance(current_point, start)
            d_end = distance(current_point, end)

            if d_start < best_distance:
                best_idx = idx
                best_distance = d_start
                best_reversed = False

            # For closed contours, reversing orientation is harmless but often unnecessary.
            if allow_reverse_open_paths and not getattr(path, "closed", False) and d_end < best_distance:
                best_idx = idx
                best_distance = d_end
                best_reversed = True

        chosen = remaining.pop(best_idx)
        if best_reversed:
            chosen.points.reverse()
            # IMPORTANT: do not force closed=False. Reversing does not change closure.
            if hasattr(chosen, "calculate_length"):
                chosen.calculate_length()

        ordered.append(chosen)
        current_point = (chosen.points[-1].x, chosen.points[-1].y)

    return ordered


def optimize_paths(paths: List, start_point: Tuple[float, float] = (0.0, 0.0), min_distance: float = 0.25, min_length_mm: float = 1.0) -> List:
    cleaned = [remove_redundant_points(p, min_distance=min_distance) for p in paths]
    cleaned = filter_short_paths(cleaned, min_length_mm=min_length_mm)
    return order_paths(cleaned, start_point=start_point)


if __name__ == "__main__":
    print("path_optimizer loaded")
