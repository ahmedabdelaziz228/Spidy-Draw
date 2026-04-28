"""
web_handlers.py - Python reference API for image vectorization + G-code generation.

This is not meant to run on the ESP32. It documents/validates the browser/server-side
image-to-path flow before sending G-code to the existing ESP32 /upload endpoint.
"""

from __future__ import annotations

import json
import os
import tempfile
from typing import Any, Dict, List

from image_processor import ImageProcessor
from path_optimizer import optimize_paths
from preset_modes import PresetModes


class WebHandlers:
    def __init__(self, image_processor: ImageProcessor = None):
        self.processor = image_processor or ImageProcessor()
        self.last_result = None
        self.last_paths = None

    def configure_processor(self, params: Dict[str, Any]) -> None:
        preset_name = params.get("preset")
        if preset_name:
            preset = PresetModes.get(preset_name)
            if not preset:
                raise ValueError(f"Unknown preset: {preset_name}")
            kwargs = preset.to_processor_kwargs()
        else:
            kwargs = {}

        # User overrides.
        allowed = {
            "threshold_value", "auto_threshold", "invert", "simplify_tolerance",
            "min_path_length", "min_point_distance_mm", "min_area_px", "morph_kernel",
            "workspace_width_mm", "workspace_height_mm", "processing_width_px",
            "retrieve_mode", "blur_kernel",
        }
        for key in allowed:
            if key in params:
                kwargs[key] = params[key]

        if kwargs:
            self.processor = ImageProcessor(**kwargs)

    def handle_process_image(self, image_bytes: bytes, filename: str, params: Dict[str, Any] = None) -> Dict[str, Any]:
        params = params or {}
        try:
            self.configure_processor(params)
            suffix = os.path.splitext(filename)[1] or ".png"
            with tempfile.NamedTemporaryFile(delete=False, suffix=suffix) as tmp:
                tmp.write(image_bytes)
                tmp_path = tmp.name

            try:
                result = self.processor.process(tmp_path)
            finally:
                try:
                    os.unlink(tmp_path)
                except FileNotFoundError:
                    pass

            if not result:
                return {
                    "success": False,
                    "error": "No drawable paths extracted. Try another preset, invert, or threshold.",
                    "code": "PROCESSING_FAILED",
                }

            self.last_result = result
            self.last_paths = result.paths
            return {
                "success": True,
                "data": result.to_dict(),
                "message": f"Extracted {len(result.paths)} clean path(s).",
            }
        except Exception as exc:
            return {"success": False, "error": str(exc), "code": "EXCEPTION"}

    def handle_validate_paths(self) -> Dict[str, Any]:
        if not self.last_result:
            return {"success": False, "error": "No paths loaded", "code": "NO_PATHS"}

        issues = []
        for i, path in enumerate(self.last_result.paths):
            if not path.is_valid(self.last_result.workspace_width_mm, self.last_result.workspace_height_mm):
                issues.append(f"Path {i}: invalid/out-of-bounds point(s).")
            if path.is_trivial(0.5):
                issues.append(f"Path {i}: too short ({path.length_mm:.2f} mm).")

        if self.last_result.total_points > 3500:
            issues.append("Too many points; increase simplify_tolerance before sending to the robot.")

        return {
            "success": True,
            "data": {
                "path_count": len(self.last_result.paths),
                "total_points": self.last_result.total_points,
                "total_length_mm": round(self.last_result.total_length_mm, 2),
                "bounds": self.last_result.to_dict()["bounds"],
                "has_issues": bool(issues),
                "issues": issues,
            },
            "message": f"Validation complete: {len(issues)} issue(s).",
        }

    def handle_preview(self) -> Dict[str, Any]:
        if not self.last_result:
            return {"success": False, "error": "No paths loaded", "code": "NO_PATHS"}

        svg_paths = []
        for i, path in enumerate(self.last_result.paths):
            if not path.points:
                continue
            d = f"M{path.points[0].x:.2f},{path.points[0].y:.2f}"
            for point in path.points[1:]:
                d += f" L{point.x:.2f},{point.y:.2f}"
            if path.closed:
                d += " Z"
            svg_paths.append({
                "id": f"path_{i}",
                "d": d,
                "closed": path.closed,
                "points": len(path.points),
                "length_mm": round(path.length_mm, 2),
                "start": path.points[0].to_dict(),
                "end": path.points[-1].to_dict(),
            })

        return {"success": True, "data": {"svg_paths": svg_paths, "bounds": self.last_result.to_dict()["bounds"]}}

    def handle_generate_gcode(self, generation_params: Dict[str, Any]) -> Dict[str, Any]:
        if not self.last_paths:
            return {"success": False, "error": "No paths loaded. Process an image first.", "code": "NO_PATHS"}

        try:
            safe_margin = float(generation_params.get("safe_margin", 5.0))
            optimize = bool(generation_params.get("optimize", True))
            max_commands = int(generation_params.get("max_commands", 8000))

            paths = list(self.last_paths)
            if optimize:
                paths = optimize_paths(
                    paths,
                    start_point=(safe_margin, safe_margin),
                    min_distance=float(generation_params.get("min_point_distance_mm", 0.25)),
                    min_length_mm=float(generation_params.get("min_path_length", 1.0)),
                )

            gcode_lines = self._generate_gcode(paths, safe_margin=safe_margin)
            stats = self._calculate_gcode_stats(gcode_lines)

            if len(gcode_lines) > max_commands:
                return {
                    "success": False,
                    "error": f"Generated command count ({len(gcode_lines)}) exceeds limit ({max_commands}). Increase simplification.",
                    "code": "TOO_MANY_COMMANDS",
                    "data": {"stats": stats},
                }

            return {
                "success": True,
                "data": {"gcode": gcode_lines, "stats": stats, "command_count": len(gcode_lines)},
                "message": f"Generated {len(gcode_lines)} ESP-compatible G-code commands.",
            }
        except Exception as exc:
            return {"success": False, "error": str(exc), "code": "EXCEPTION"}

    def _fit_transform(self, paths, safe_margin: float):
        xs = [pt.x for p in paths for pt in p.points]
        ys = [pt.y for p in paths for pt in p.points]
        if not xs or not ys:
            return 1.0, 0.0, 0.0
        min_x, max_x = min(xs), max(xs)
        min_y, max_y = min(ys), max(ys)
        draw_w = max(max_x - min_x, 1e-6)
        draw_h = max(max_y - min_y, 1e-6)
        workspace_w = self.processor.workspace_width_mm
        workspace_h = self.processor.workspace_height_mm
        available_w = max(1.0, workspace_w - 2 * safe_margin)
        available_h = max(1.0, workspace_h - 2 * safe_margin)
        scale = min(available_w / draw_w, available_h / draw_h, 1.0)
        offset_x = safe_margin + (available_w - draw_w * scale) / 2.0 - min_x * scale
        offset_y = safe_margin + (available_h - draw_h * scale) / 2.0 - min_y * scale
        return scale, offset_x, offset_y

    def _generate_gcode(self, paths, safe_margin: float = 5.0) -> List[str]:
        """Generate only commands supported by the ESP parser: G0/G1/M3/M5."""
        commands: List[str] = ["M5"]
        scale, offset_x, offset_y = self._fit_transform(paths, safe_margin)
        workspace_w = self.processor.workspace_width_mm
        workspace_h = self.processor.workspace_height_mm

        pen_down = False
        last_xy = None

        for path in paths:
            if len(path.points) < 2:
                continue

            first = True
            for point in path.points:
                x = min(max(point.x * scale + offset_x, 0.0), workspace_w)
                y = min(max(point.y * scale + offset_y, 0.0), workspace_h)
                xy = (round(x, 2), round(y, 2))

                if first:
                    if pen_down:
                        commands.append("M5")
                        pen_down = False
                    if last_xy != xy:
                        commands.append(f"G0 X{xy[0]:.2f} Y{xy[1]:.2f}")
                    commands.append("M3")
                    pen_down = True
                    first = False
                else:
                    if last_xy == xy:
                        continue
                    commands.append(f"G1 X{xy[0]:.2f} Y{xy[1]:.2f}")

                last_xy = xy

            if path.closed and path.points:
                start = path.points[0]
                x = min(max(start.x * scale + offset_x, 0.0), workspace_w)
                y = min(max(start.y * scale + offset_y, 0.0), workspace_h)
                xy = (round(x, 2), round(y, 2))
                if last_xy != xy:
                    commands.append(f"G1 X{xy[0]:.2f} Y{xy[1]:.2f}")
                    last_xy = xy

            if pen_down:
                commands.append("M5")
                pen_down = False

        if commands[-1] != "M5":
            commands.append("M5")
        return commands

    @staticmethod
    def _calculate_gcode_stats(gcode_lines: List[str]) -> Dict[str, Any]:
        moves = sum(1 for line in gcode_lines if line.startswith("G0") or line.startswith("G1"))
        return {
            "total_commands": len(gcode_lines),
            "move_commands": moves,
            "rapid_moves": sum(1 for line in gcode_lines if line.startswith("G0")),
            "draw_moves": sum(1 for line in gcode_lines if line.startswith("G1")),
            "pen_up_commands": sum(1 for line in gcode_lines if line == "M5"),
            "pen_down_commands": sum(1 for line in gcode_lines if line == "M3"),
            "estimated_time_sec": round(moves * 0.12, 1),
        }


if __name__ == "__main__":
    print(json.dumps(PresetModes.list_all(), indent=2, ensure_ascii=False))
