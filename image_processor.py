"""
image_processor.py - Image Processing / Vectorization Module

Muhammad's completed part: image -> clean vector paths.

Key fixes in this version:
- Correct foreground extraction for black drawings on white backgrounds using THRESH_BINARY_INV.
- Separate image pixel-space from robot workspace mm-space.
- Scale all extracted contour points into workspace coordinates.
- Add stronger contour filtering, duplicate removal, and path cleanup.
- Keep path output format stable for Fathy's G-code stage:
    [{"x": float, "y": float}, ...] per path.

Expected pipeline:
    Image -> grayscale -> blur -> threshold/invert -> morphology cleanup
    -> contour extraction -> pixel-to-mm mapping -> simplification
    -> filtering -> ordered clean VectorPath objects.
"""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple

import cv2
import numpy as np


@dataclass
class Point:
    """Single point in robot workspace coordinates, in millimeters."""

    x: float
    y: float

    def to_dict(self) -> Dict[str, float]:
        return {"x": round(float(self.x), 3), "y": round(float(self.y), 3)}

    @staticmethod
    def from_tuple(t: Tuple[float, float]) -> "Point":
        return Point(float(t[0]), float(t[1]))

    def distance_to(self, other: "Point") -> float:
        return float(((self.x - other.x) ** 2 + (self.y - other.y) ** 2) ** 0.5)


@dataclass
class VectorPath:
    """A vector path ready for preview / G-code generation."""

    points: List[Point]
    closed: bool = False
    length_mm: float = 0.0
    original_point_count: int = 0
    is_simplified: bool = False
    simplify_tolerance: float = 0.0
    source_area_px: float = 0.0

    def to_dict(self) -> Dict:
        return {
            "points": [p.to_dict() for p in self.points],
            "closed": bool(self.closed),
            "length_mm": round(float(self.length_mm), 3),
            "original_point_count": int(self.original_point_count),
            "is_simplified": bool(self.is_simplified),
            "simplify_tolerance": round(float(self.simplify_tolerance), 3),
            "point_count": len(self.points),
            "source_area_px": round(float(self.source_area_px), 2),
        }

    def calculate_length(self) -> float:
        self.length_mm = 0.0
        if len(self.points) < 2:
            return 0.0
        for i in range(1, len(self.points)):
            self.length_mm += self.points[i - 1].distance_to(self.points[i])
        if self.closed and len(self.points) > 2:
            # Avoid double-counting if the path is already explicitly closed.
            if self.points[-1].distance_to(self.points[0]) > 1e-6:
                self.length_mm += self.points[-1].distance_to(self.points[0])
        return self.length_mm

    def get_bounds(self) -> Tuple[float, float, float, float]:
        if not self.points:
            return 0.0, 0.0, 0.0, 0.0
        xs = [p.x for p in self.points]
        ys = [p.y for p in self.points]
        return min(xs), min(ys), max(xs), max(ys)

    def is_valid(self, workspace_width_mm: float = 200.0, workspace_height_mm: float = 200.0) -> bool:
        if len(self.points) < 2:
            return False
        for p in self.points:
            if not np.isfinite(p.x) or not np.isfinite(p.y):
                return False
            if p.x < -0.001 or p.y < -0.001:
                return False
            if p.x > workspace_width_mm + 0.001 or p.y > workspace_height_mm + 0.001:
                return False
        return True

    def is_trivial(self, min_length_mm: float = 0.5) -> bool:
        return self.length_mm < min_length_mm


@dataclass
class PathExtractionResult:
    """Complete result from image vectorization."""

    paths: List[VectorPath]
    total_length_mm: float = 0.0
    total_points: int = 0
    min_x: float = 0.0
    min_y: float = 0.0
    max_x: float = 0.0
    max_y: float = 0.0
    has_closed_paths: bool = False
    has_open_paths: bool = False
    source_image: str = ""
    processing_notes: str = ""
    workspace_width_mm: float = 200.0
    workspace_height_mm: float = 200.0
    original_size_px: Tuple[int, int] = (0, 0)
    processing_size_px: Tuple[int, int] = (0, 0)
    rejected_contours: int = 0

    def calculate_stats(self) -> None:
        if not self.paths:
            self.total_length_mm = 0.0
            self.total_points = 0
            self.has_closed_paths = False
            self.has_open_paths = False
            self.min_x = self.min_y = self.max_x = self.max_y = 0.0
            return

        for path in self.paths:
            path.calculate_length()

        self.total_length_mm = sum(p.length_mm for p in self.paths)
        self.total_points = sum(len(p.points) for p in self.paths)
        self.has_closed_paths = any(p.closed for p in self.paths)
        self.has_open_paths = any(not p.closed for p in self.paths)

        bounds = [p.get_bounds() for p in self.paths]
        self.min_x = min(b[0] for b in bounds)
        self.min_y = min(b[1] for b in bounds)
        self.max_x = max(b[2] for b in bounds)
        self.max_y = max(b[3] for b in bounds)

    def is_valid(self) -> bool:
        if not self.paths:
            return False
        return all(p.is_valid(self.workspace_width_mm, self.workspace_height_mm) for p in self.paths)

    def to_dict(self) -> Dict:
        return {
            "paths": [p.to_dict() for p in self.paths],
            "path_count": len(self.paths),
            "total_length_mm": round(self.total_length_mm, 3),
            "total_points": self.total_points,
            "bounds": {
                "minX": round(self.min_x, 3),
                "minY": round(self.min_y, 3),
                "maxX": round(self.max_x, 3),
                "maxY": round(self.max_y, 3),
            },
            "workspace": {
                "width_mm": self.workspace_width_mm,
                "height_mm": self.workspace_height_mm,
            },
            "image": {
                "source": self.source_image,
                "original_size_px": self.original_size_px,
                "processing_size_px": self.processing_size_px,
            },
            "has_closed_paths": self.has_closed_paths,
            "has_open_paths": self.has_open_paths,
            "rejected_contours": self.rejected_contours,
            "processing_notes": self.processing_notes.strip(),
        }


class ImageProcessor:
    """Image-to-vector path processor for the real robot workflow."""

    def __init__(
        self,
        target_width: int = 200,
        target_height: int = 200,
        blur_kernel: int = 5,
        threshold_value: int = 127,
        simplify_tolerance: float = 0.8,
        min_path_length: float = 1.0,
        *,
        workspace_width_mm: Optional[float] = None,
        workspace_height_mm: Optional[float] = None,
        processing_width_px: int = 512,
        processing_height_px: Optional[int] = None,
        invert: bool = True,
        auto_threshold: bool = False,
        min_area_px: float = 8.0,
        min_point_distance_mm: float = 0.25,
        close_tolerance_mm: float = 0.75,
        retrieve_mode: str = "external",
        morph_kernel: int = 3,
        flip_y: bool = False,
    ):
        # Backward-compatible names: target_width/height are the intended robot workspace size.
        self.workspace_width_mm = float(workspace_width_mm if workspace_width_mm is not None else target_width)
        self.workspace_height_mm = float(workspace_height_mm if workspace_height_mm is not None else target_height)

        self.processing_width_px = int(processing_width_px)
        if processing_height_px is None:
            # Preserve aspect of robot workspace by default.
            self.processing_height_px = max(1, int(round(self.processing_width_px * self.workspace_height_mm / self.workspace_width_mm)))
        else:
            self.processing_height_px = int(processing_height_px)

        self.target_width = int(target_width)
        self.target_height = int(target_height)
        self.blur_kernel = self._make_odd(blur_kernel)
        self.threshold_value = int(threshold_value)
        self.simplify_tolerance = float(simplify_tolerance)
        self.min_path_length = float(min_path_length)
        self.invert = bool(invert)
        self.auto_threshold = bool(auto_threshold)
        self.min_area_px = float(min_area_px)
        self.min_point_distance_mm = float(min_point_distance_mm)
        self.close_tolerance_mm = float(close_tolerance_mm)
        self.retrieve_mode = retrieve_mode
        self.morph_kernel = self._make_odd(morph_kernel) if morph_kernel > 0 else 0
        self.flip_y = bool(flip_y)

        self._last_binary: Optional[np.ndarray] = None
        self._last_original_size: Tuple[int, int] = (0, 0)
        self._last_processing_size: Tuple[int, int] = (self.processing_width_px, self.processing_height_px)

    @staticmethod
    def _make_odd(value: int) -> int:
        value = max(1, int(value))
        return value if value % 2 == 1 else value + 1

    def load_image(self, image_path: str) -> Optional[np.ndarray]:
        image = cv2.imread(image_path, cv2.IMREAD_COLOR)
        if image is None:
            print(f"Error: could not load image from {image_path}")
            return None
        return image

    def preprocess_image(self, image: np.ndarray) -> np.ndarray:
        """Return binary image where foreground strokes are white (255)."""
        h, w = image.shape[:2]
        self._last_original_size = (int(w), int(h))

        resized = cv2.resize(
            image,
            (self.processing_width_px, self.processing_height_px),
            interpolation=cv2.INTER_AREA if max(w, h) > max(self.processing_width_px, self.processing_height_px) else cv2.INTER_CUBIC,
        )
        gray = cv2.cvtColor(resized, cv2.COLOR_BGR2GRAY)

        if self.blur_kernel > 1:
            gray = cv2.GaussianBlur(gray, (self.blur_kernel, self.blur_kernel), 0)

        threshold_type = cv2.THRESH_BINARY_INV if self.invert else cv2.THRESH_BINARY
        if self.auto_threshold:
            _, binary = cv2.threshold(gray, 0, 255, threshold_type | cv2.THRESH_OTSU)
        else:
            _, binary = cv2.threshold(gray, self.threshold_value, 255, threshold_type)

        if self.morph_kernel > 1:
            kernel = np.ones((self.morph_kernel, self.morph_kernel), np.uint8)
            # IMPORTANT FIX:
            # Do NOT apply MORPH_OPEN by default on thin line-art drawings.
            # Opening erodes fine strokes (circles/signatures) and can collapse
            # a whole contour into tiny fragments or short line segments.
            # A light CLOSE is much safer here: it reconnects small gaps
            # without destroying the original path topology.
            binary = cv2.morphologyEx(binary, cv2.MORPH_CLOSE, kernel, iterations=1)

        self._last_binary = binary
        self._last_processing_size = (int(binary.shape[1]), int(binary.shape[0]))
        return binary

    def find_contours(self, binary_image: np.ndarray) -> List[np.ndarray]:
        mode = cv2.RETR_EXTERNAL if self.retrieve_mode.lower() == "external" else cv2.RETR_TREE
        contours, _hierarchy = cv2.findContours(binary_image, mode, cv2.CHAIN_APPROX_NONE)
        return list(contours)

    def _pixel_to_workspace(self, x_px: float, y_px: float) -> Point:
        scale_x = self.workspace_width_mm / max(1, self.processing_width_px - 1)
        scale_y = self.workspace_height_mm / max(1, self.processing_height_px - 1)
        x_mm = x_px * scale_x
        y_mm = y_px * scale_y
        if self.flip_y:
            y_mm = self.workspace_height_mm - y_mm
        x_mm = min(max(x_mm, 0.0), self.workspace_width_mm)
        y_mm = min(max(y_mm, 0.0), self.workspace_height_mm)
        return Point(float(x_mm), float(y_mm))

    def contour_to_points(self, contour: np.ndarray) -> List[Point]:
        points: List[Point] = []
        for pt in contour:
            x, y = float(pt[0][0]), float(pt[0][1])
            points.append(self._pixel_to_workspace(x, y))
        return points

    def remove_duplicates(self, points: List[Point], min_distance: Optional[float] = None) -> List[Point]:
        if not points:
            return []
        threshold = self.min_point_distance_mm if min_distance is None else float(min_distance)
        filtered = [points[0]]
        for p in points[1:]:
            if filtered[-1].distance_to(p) >= threshold:
                filtered.append(p)
        # If explicitly closed and the duplicate final point is very close to first, remove it.
        if len(filtered) > 2 and filtered[0].distance_to(filtered[-1]) < threshold:
            filtered.pop()
        return filtered

    def douglas_peucker(self, points: List[Point], epsilon: float) -> List[Point]:
        if len(points) < 3:
            return list(points)

        max_dist = 0.0
        max_idx = 0
        for i in range(1, len(points) - 1):
            dist = self._point_line_distance(points[i], points[0], points[-1])
            if dist > max_dist:
                max_dist = dist
                max_idx = i

        if max_dist > epsilon:
            left = self.douglas_peucker(points[: max_idx + 1], epsilon)
            right = self.douglas_peucker(points[max_idx:], epsilon)
            return left[:-1] + right
        return [points[0], points[-1]]

    @staticmethod
    def _point_line_distance(point: Point, line_start: Point, line_end: Point) -> float:
        px, py = point.x, point.y
        x1, y1 = line_start.x, line_start.y
        x2, y2 = line_end.x, line_end.y
        denom = ((y2 - y1) ** 2 + (x2 - x1) ** 2) ** 0.5
        if denom == 0:
            return point.distance_to(line_start)
        return abs((y2 - y1) * px - (x2 - x1) * py + x2 * y1 - y2 * x1) / denom

    def _clean_path_points(self, points: List[Point], closed: bool) -> List[Point]:
        points = self.remove_duplicates(points)
        if len(points) < 2:
            return []

        # Simplify closed contours without making the first/last line degenerate.
        if closed and len(points) > 3:
            simplified = cv2.approxPolyDP(
                np.array([[[p.x, p.y]] for p in points], dtype=np.float32),
                epsilon=self.simplify_tolerance,
                closed=True,
            )
            cleaned = [Point(float(p[0][0]), float(p[0][1])) for p in simplified]
        else:
            cleaned = self.douglas_peucker(points, self.simplify_tolerance)

        cleaned = self.remove_duplicates(cleaned)
        return cleaned

    def process(self, image_path: str) -> Optional[PathExtractionResult]:
        image = self.load_image(image_path)
        if image is None:
            return None

        binary = self.preprocess_image(image)
        contours = self.find_contours(binary)

        result = PathExtractionResult(
            paths=[],
            source_image=Path(image_path).name,
            workspace_width_mm=self.workspace_width_mm,
            workspace_height_mm=self.workspace_height_mm,
            original_size_px=self._last_original_size,
            processing_size_px=self._last_processing_size,
        )

        paths: List[VectorPath] = []
        rejected = 0

        # Largest contours first makes output easier to inspect and avoids tiny noise first.
        contours = sorted(contours, key=cv2.contourArea, reverse=True)

        for contour in contours:
            if len(contour) < 2:
                rejected += 1
                continue

            area_px = float(abs(cv2.contourArea(contour)))
            if area_px < self.min_area_px:
                rejected += 1
                continue

            points = self.contour_to_points(contour)
            points = self.remove_duplicates(points)
            if len(points) < 2:
                rejected += 1
                continue

            original_count = len(points)
            # OpenCV contours represent boundaries; treat them as closed if endpoints are near or area exists.
            closed = area_px > self.min_area_px and len(points) > 3
            cleaned = self._clean_path_points(points, closed=closed)
            if len(cleaned) < 2:
                rejected += 1
                continue

            path = VectorPath(
                points=cleaned,
                closed=closed,
                original_point_count=original_count,
                is_simplified=True,
                simplify_tolerance=self.simplify_tolerance,
                source_area_px=area_px,
            )
            path.calculate_length()

            if path.is_trivial(self.min_path_length):
                rejected += 1
                continue
            if not path.is_valid(self.workspace_width_mm, self.workspace_height_mm):
                rejected += 1
                continue

            paths.append(path)

        result.paths = paths
        result.rejected_contours = rejected
        result.calculate_stats()

        if not result.is_valid():
            result.processing_notes = "No valid drawable paths were extracted. Try invert/threshold/preset settings."
            print("Warning: no valid paths extracted from image")
            return None

        result.processing_notes = (
            f"Foreground={'black strokes' if self.invert else 'white strokes'}, "
            f"threshold={'Otsu' if self.auto_threshold else self.threshold_value}, "
            f"rejected_contours={rejected}."
        )
        return result

    def process_array(self, image: np.ndarray, source_name: str = "array") -> Optional[PathExtractionResult]:
        """Useful for tests or browser/server integrations where an image is already decoded."""
        tmp = Path("/tmp") / f"image_processor_{source_name}.png"
        cv2.imwrite(str(tmp), image)
        try:
            return self.process(str(tmp))
        finally:
            try:
                tmp.unlink()
            except FileNotFoundError:
                pass


if __name__ == "__main__":
    # Smoke test: black square on white background.
    test_image_path = "/tmp/test_drawing.png"
    test_img = np.ones((240, 320, 3), dtype=np.uint8) * 255
    cv2.rectangle(test_img, (80, 60), (240, 180), (0, 0, 0), 3)
    cv2.imwrite(test_image_path, test_img)

    processor = ImageProcessor(auto_threshold=True, invert=True, simplify_tolerance=0.6)
    output = processor.process(test_image_path)
    if output:
        print(json.dumps(output.to_dict(), indent=2, ensure_ascii=False))
    else:
        print("No paths extracted")
