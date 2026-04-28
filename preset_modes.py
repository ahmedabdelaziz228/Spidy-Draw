"""
preset_modes.py - Stable presets for Muhammad's image vectorization stage.

All distances below are in robot workspace millimeters after pixel-to-mm scaling.
"""

from dataclasses import dataclass
from typing import Optional


@dataclass
class PresetConfig:
    name: str
    target_width: int = 200
    target_height: int = 200
    workspace_width_mm: float = 200.0
    workspace_height_mm: float = 200.0
    processing_width_px: int = 512
    blur_kernel: int = 5
    threshold_value: int = 127
    auto_threshold: bool = False
    invert: bool = True
    simplify_tolerance: float = 0.8
    min_path_length: float = 1.0
    min_point_distance_mm: float = 0.25
    min_area_px: float = 8.0
    morph_kernel: int = 3
    retrieve_mode: str = "external"
    description: str = ""

    def to_processor_kwargs(self) -> dict:
        return {
            "target_width": self.target_width,
            "target_height": self.target_height,
            "workspace_width_mm": self.workspace_width_mm,
            "workspace_height_mm": self.workspace_height_mm,
            "processing_width_px": self.processing_width_px,
            "blur_kernel": self.blur_kernel,
            "threshold_value": self.threshold_value,
            "auto_threshold": self.auto_threshold,
            "invert": self.invert,
            "simplify_tolerance": self.simplify_tolerance,
            "min_path_length": self.min_path_length,
            "min_point_distance_mm": self.min_point_distance_mm,
            "min_area_px": self.min_area_px,
            "morph_kernel": self.morph_kernel,
            "retrieve_mode": self.retrieve_mode,
        }


class PresetModes:
    LOGO = PresetConfig(
        name="logo",
        blur_kernel=5,
        threshold_value=150,
        auto_threshold=False,
        invert=True,
        simplify_tolerance=1.2,
        min_path_length=2.0,
        min_point_distance_mm=0.35,
        min_area_px=20.0,
        morph_kernel=3,
        description="Clean high-contrast logos with fewer points.",
    )

    SIGNATURE = PresetConfig(
        name="signature",
        blur_kernel=3,
        threshold_value=170,
        auto_threshold=False,
        invert=True,
        simplify_tolerance=0.45,
        min_path_length=0.6,
        min_point_distance_mm=0.12,
        min_area_px=3.0,
        morph_kernel=1,
        description="Thin black signatures or handwriting on white background.",
    )

    FINE_ART = PresetConfig(
        name="fine-art",
        blur_kernel=3,
        threshold_value=127,
        auto_threshold=True,
        invert=True,
        simplify_tolerance=0.25,
        min_path_length=0.5,
        min_point_distance_mm=0.08,
        min_area_px=2.0,
        morph_kernel=1,
        description="Keeps more detail; may generate many points.",
    )

    OUTLINE = PresetConfig(
        name="outline",
        blur_kernel=7,
        threshold_value=145,
        auto_threshold=False,
        invert=True,
        simplify_tolerance=1.8,
        min_path_length=3.0,
        min_point_distance_mm=0.45,
        min_area_px=30.0,
        morph_kernel=3,
        retrieve_mode="external",
        description="Outer outlines only; best first test mode for the robot.",
    )

    _MAP = {
        "logo": LOGO,
        "signature": SIGNATURE,
        "fine-art": FINE_ART,
        "fine_art": FINE_ART,
        "outline": OUTLINE,
    }

    @classmethod
    def get(cls, name: str) -> Optional[PresetConfig]:
        return cls._MAP.get((name or "").lower())

    @classmethod
    def list_all(cls) -> dict:
        return {
            key: {
                "description": cfg.description,
                "invert": cfg.invert,
                "auto_threshold": cfg.auto_threshold,
                "threshold_value": cfg.threshold_value,
                "simplify_tolerance_mm": cfg.simplify_tolerance,
                "min_path_length_mm": cfg.min_path_length,
                "min_point_distance_mm": cfg.min_point_distance_mm,
                "min_area_px": cfg.min_area_px,
            }
            for key, cfg in cls._MAP.items()
            if key != "fine_art"
        }


if __name__ == "__main__":
    for name, cfg in PresetModes.list_all().items():
        print(name, cfg)
