"""
gcode_exporter.py - Export G-code and path data for debugging and analysis

Provides functions to export generated G-code and path data in multiple formats:
- JSON: Path data with full metadata
- SVG: Visual representation of paths
- GCode: Standard G-code format (with comments)
- Stats: Performance and quality metrics
"""

import json
from typing import List, Dict, Any, Optional
from datetime import datetime


class GCodeExporter:
    """Export G-code and path data in multiple formats"""

    @staticmethod
    def export_paths_json(paths, with_metadata: bool = True) -> str:
        """
        Export paths as JSON

        Args:
            paths: List of VectorPath objects
            with_metadata: Include timing and statistics

        Returns:
            JSON string
        """
        data = {
            "type": "path_collection",
            "version": "1.0",
            "timestamp": datetime.now().isoformat(),
            "path_count": len(paths),
            "paths": [p.to_dict() if hasattr(p, 'to_dict') else {
                "points": [(pt.x, pt.y) for pt in p.points],
                "closed": p.closed,
                "length_mm": sum(p.points[i-1].distance_to(p.points[i])
                                for i in range(1, len(p.points))) if len(p.points) > 1 else 0,
                "point_count": len(p.points)
            } for p in paths]
        }

        if with_metadata:
            total_points = sum(len(p.points) if hasattr(p, 'points') else 0 for p in paths)
            data["metadata"] = {
                "total_points": total_points,
                "total_length_mm": sum(
                    p.length_mm if hasattr(p, 'length_mm') else
                    sum(p.points[i-1].distance_to(p.points[i])
                        for i in range(1, len(p.points))) if len(p.points) > 1 else 0
                    for p in paths
                )
            }

        return json.dumps(data, indent=2)

    @staticmethod
    def export_gcode_with_comments(gcode_lines: List[str]) -> str:
        """
        Export G-code with informative comments

        Args:
            gcode_lines: List of G-code command strings

        Returns:
            G-code string with comments and metadata
        """
        lines = [
            "; Motion Control Robot - Auto-Generated G-code",
            f"; Generated: {datetime.now().isoformat()}",
            "; WARNING: Verify bounds and preview before execution",
            ";",
            f"; Total commands: {len(gcode_lines)}",
            "; Format: G0/G1 for motion, M3/M5 for pen control",
            ";",
        ]

        # Add command statistics
        move_count = sum(1 for line in gcode_lines if line.startswith("G"))
        pen_down = sum(1 for line in gcode_lines if line == "M3")
        pen_up = sum(1 for line in gcode_lines if line == "M5")

        lines.extend([
            f"; Movement commands (G0/G1): {move_count}",
            f"; Pen DOWN commands (M3): {pen_down}",
            f"; Pen UP commands (M5): {pen_up}",
            ";",
        ])

        # Add the actual G-code
        lines.extend(gcode_lines)

        return "\n".join(lines)

    @staticmethod
    def export_svg_preview(paths, width: int = 400, height: int = 400,
                           scale: Optional[float] = None) -> str:
        """
        Export paths as SVG for visualization

        Args:
            paths: List of VectorPath objects
            width: SVG canvas width
            height: SVG canvas height
            scale: Optional scaling factor

        Returns:
            SVG XML string
        """
        # Calculate bounds
        all_x = []
        all_y = []
        for p in paths:
            for pt in p.points:
                all_x.append(pt.x)
                all_y.append(pt.y)

        if not all_x:
            return '<svg width="400" height="400"><text>No paths</text></svg>'

        min_x, max_x = min(all_x), max(all_x)
        min_y, max_y = min(all_y), max(all_y)

        # Auto-scale if needed
        if scale is None:
            scale_x = (width - 40) / (max_x - min_x) if max_x > min_x else 1.0
            scale_y = (height - 40) / (max_y - min_y) if max_y > min_y else 1.0
            scale = min(scale_x, scale_y, 1.0)

        # SVG header
        svg = [f'<svg width="{width}" height="{height}" xmlns="http://www.w3.org/2000/svg">']
        svg.append('<style>path { stroke-width: 2; fill: none; } .end-point { fill: red; r: 3; }</style>')

        # Background
        svg.append(f'<rect width="{width}" height="{height}" fill="white"/>')

        # Grid (optional)
        svg.append('<g stroke="#eee" stroke-width="0.5">')
        for i in range(0, 200, 20):
            sx = 20 + (i - min_x) * scale
            sy = 20 + (i - min_y) * scale
            svg.append(f'<line x1="{sx}" y1="20" x2="{sx}" y2="{height-20}"/>')
            svg.append(f'<line x1="20" y1="{sy}" x2="{width-20}" y2="{sy}"/>')
        svg.append('</g>')

        # Draw paths
        colors = ["#FF6B6B", "#4ECDC4", "#45B7D1", "#FFA07A", "#98D8C8"]
        for idx, p in enumerate(paths):
            color = colors[idx % len(colors)]
            path_d = "M"

            for pt_idx, pt in enumerate(p.points):
                x = 20 + (pt.x - min_x) * scale
                y = 20 + (pt.y - min_y) * scale

                if pt_idx == 0:
                    path_d += f"{x},{y}"
                else:
                    path_d += f"L{x},{y}"

            if p.closed:
                path_d += "Z"

            svg.append(f'<path d="{path_d}" stroke="{color}"/>')

            # Mark start point
            if p.points:
                start_x = 20 + (p.points[0].x - min_x) * scale
                start_y = 20 + (p.points[0].y - min_y) * scale
                svg.append(f'<circle cx="{start_x}" cy="{start_y}" r="4" fill="green" opacity="0.8"/>')

                # Mark end point
                end_x = 20 + (p.points[-1].x - min_x) * scale
                end_y = 20 + (p.points[-1].y - min_y) * scale
                svg.append(f'<circle cx="{end_x}" cy="{end_y}" r="3" fill="red" opacity="0.6"/>')

        # Legend
        svg.append('<g font-size="12">')
        svg.append('<text x="20" y="15" fill="black">Green: Start | Red: End</text>')
        svg.append('</g>')

        svg.append('</svg>')
        return "\n".join(svg)

    @staticmethod
    def export_stats(paths, gcode_lines: Optional[List[str]] = None) -> Dict[str, Any]:
        """
        Export processing and generation statistics

        Args:
            paths: List of VectorPath objects
            gcode_lines: Optional list of G-code lines

        Returns:
            Dictionary with statistics
        """
        stats = {
            "paths": {
                "count": len(paths),
                "closed": sum(1 for p in paths if p.closed),
                "open": sum(1 for p in paths if not p.closed),
                "total_points": sum(len(p.points) for p in paths),
                "total_length_mm": sum(p.length_mm if hasattr(p, 'length_mm') else
                                      sum(p.points[i-1].distance_to(p.points[i])
                                          for i in range(1, len(p.points)))
                                      for p in paths if len(p.points) > 1),
                "avg_points_per_path": sum(len(p.points) for p in paths) / len(paths) if paths else 0,
            }
        }

        if gcode_lines:
            stats["gcode"] = {
                "total_commands": len(gcode_lines),
                "move_commands": sum(1 for line in gcode_lines if line.startswith("G")),
                "pen_down": sum(1 for line in gcode_lines if line == "M3"),
                "pen_up": sum(1 for line in gcode_lines if line == "M5"),
                "estimated_time_seconds": sum(1 for line in gcode_lines if line.startswith("G")) * 0.1
            }

        return stats


if __name__ == "__main__":
    print("GCode Exporter module loaded. Use in web handlers to export results.")
