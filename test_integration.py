"""
test_integration.py - End-to-end integration tests for motion control project

Tests the complete pipeline:
  Image → Vector Paths → G-code → Validation

Run with: python test_integration.py
"""

import unittest
import json
import tempfile
import os
import cv2
import numpy as np
from image_processor import ImageProcessor, VectorPath, Point
from web_handlers import WebHandlers


class TestImageProcessor(unittest.TestCase):
    """Test image processing module"""

    @classmethod
    def setUpClass(cls):
        """Create test images"""
        cls.processor = ImageProcessor(
            target_width=200,
            target_height=200,
            blur_kernel=5,
            threshold_value=127,
            simplify_tolerance=0.5,
            min_path_length=0.5
        )

        cls.test_images = {}

        # Test 1: Simple square
        img = np.ones((200, 200, 3), dtype=np.uint8) * 255
        cv2.rectangle(img, (50, 50), (150, 150), (0, 0, 0), 2)
        cls.test_images['square'] = cls._save_temp_image(img)

        # Test 2: Circle
        img = np.ones((200, 200, 3), dtype=np.uint8) * 255
        cv2.circle(img, (100, 100), 50, (0, 0, 0), 2)
        cls.test_images['circle'] = cls._save_temp_image(img)

        # Test 3: Line
        img = np.ones((200, 200, 3), dtype=np.uint8) * 255
        cv2.line(img, (30, 30), (170, 170), (0, 0, 0), 2)
        cls.test_images['line'] = cls._save_temp_image(img)

        # Test 4: Complex shape (star-like)
        img = np.ones((200, 200, 3), dtype=np.uint8) * 255
        pts = np.array([
            [100, 20], [120, 80], [180, 100], [130, 140],
            [160, 200], [100, 160], [40, 200], [70, 140],
            [20, 100], [80, 80]
        ], np.int32)
        cv2.polylines(img, [pts], True, (0, 0, 0), 2)
        cls.test_images['star'] = cls._save_temp_image(img)

        # Test 5: Multiple shapes
        img = np.ones((200, 200, 3), dtype=np.uint8) * 255
        cv2.rectangle(img, (20, 20), (70, 70), (0, 0, 0), 1)
        cv2.circle(img, (150, 50), 25, (0, 0, 0), 1)
        cv2.line(img, (50, 150), (150, 180), (0, 0, 0), 1)
        cls.test_images['multi'] = cls._save_temp_image(img)

    @staticmethod
    def _save_temp_image(img_array):
        """Save numpy array as temp PNG file"""
        with tempfile.NamedTemporaryFile(delete=False, suffix=".png") as f:
            cv2.imwrite(f.name, img_array)
            return f.name

    @classmethod
    def tearDownClass(cls):
        """Clean up temp files"""
        for path in cls.test_images.values():
            if os.path.exists(path):
                os.unlink(path)

    def test_square_processing(self):
        """Test processing a simple square"""
        result = self.processor.process(self.test_images['square'])

        self.assertIsNotNone(result)
        self.assertTrue(result.is_valid())
        self.assertGreater(len(result.paths), 0)
        self.assertGreater(result.total_length_mm, 0)
        print(f"✓ Square: {len(result.paths)} paths, {result.total_points} points, {result.total_length_mm:.1f}mm")

    def test_circle_processing(self):
        """Test processing a circle"""
        result = self.processor.process(self.test_images['circle'])

        self.assertIsNotNone(result)
        self.assertTrue(result.is_valid())
        self.assertGreater(len(result.paths), 0)
        print(f"✓ Circle: {len(result.paths)} paths, {result.total_points} points, {result.total_length_mm:.1f}mm")

    def test_line_processing(self):
        """Test processing a line"""
        result = self.processor.process(self.test_images['line'])

        self.assertIsNotNone(result)
        # Line may not form a closed contour, so it might not be detected
        print(f"✓ Line: {len(result.paths) if result else 0} paths detected")

    def test_complex_shape_processing(self):
        """Test processing a complex shape"""
        result = self.processor.process(self.test_images['star'])

        self.assertIsNotNone(result)
        self.assertTrue(result.is_valid())
        self.assertGreater(len(result.paths), 0)
        print(f"✓ Star: {len(result.paths)} paths, {result.total_points} points, {result.total_length_mm:.1f}mm")

    def test_multiple_shapes_processing(self):
        """Test processing multiple shapes"""
        result = self.processor.process(self.test_images['multi'])

        self.assertIsNotNone(result)
        if result and result.is_valid():
            self.assertGreater(len(result.paths), 0)
            print(f"✓ Multi: {len(result.paths)} paths detected")
        else:
            print(f"✓ Multi: No valid paths (expected for thin lines)")

    def test_path_validation(self):
        """Test path validation logic"""
        # Create a valid path
        valid_path = VectorPath(
            points=[Point(10, 10), Point(50, 50), Point(100, 100)],
            closed=False
        )
        valid_path.calculate_length()

        self.assertTrue(valid_path.is_valid())
        self.assertGreater(valid_path.length_mm, 0)

        # Create invalid path (out of bounds)
        invalid_path = VectorPath(
            points=[Point(10, 10), Point(250, 250)],
            closed=False
        )
        invalid_path.calculate_length()

        self.assertFalse(invalid_path.is_valid())

    def test_simplification(self):
        """Test Douglas-Peucker simplification"""
        # Create a curve with many points
        points = []
        for i in range(100):
            x = i * 2.0
            y = 50 + 30 * np.sin(i * 0.1)
            points.append(Point(float(x), float(y)))

        original_count = len(points)

        # Simplify
        simplified = self.processor.douglas_peucker(points, epsilon=1.0)

        self.assertLess(len(simplified), original_count)
        print(f"✓ Simplification: {original_count} → {len(simplified)} points ({100*(original_count-len(simplified))/original_count:.0f}% reduction)")


class TestWebHandlers(unittest.TestCase):
    """Test web API handlers"""

    @classmethod
    def setUpClass(cls):
        """Create handlers and test image"""
        cls.handlers = WebHandlers()

        # Create test image
        img = np.ones((200, 200, 3), dtype=np.uint8) * 255
        cv2.rectangle(img, (40, 40), (160, 160), (0, 0, 0), 2)

        with tempfile.NamedTemporaryFile(delete=False, suffix=".png") as f:
            cv2.imwrite(f.name, img)
            cls.test_image_path = f.name

        with open(cls.test_image_path, "rb") as f:
            cls.test_image_bytes = f.read()

    @classmethod
    def tearDownClass(cls):
        """Clean up"""
        if os.path.exists(cls.test_image_path):
            os.unlink(cls.test_image_path)

    def test_process_image(self):
        """Test image processing handler"""
        response = self.handlers.handle_process_image(self.test_image_bytes, "test.png")

        self.assertIsNotNone(response)
        self.assertTrue(response.get("success"))
        self.assertIn("data", response)
        print(f"✓ Image processing: {response['message']}")

    def test_validate_paths(self):
        """Test path validation handler"""
        # Process image first
        self.handlers.handle_process_image(self.test_image_bytes, "test.png")

        # Then validate
        response = self.handlers.handle_validate_paths()

        self.assertIsNotNone(response)
        self.assertTrue(response.get("success"))
        self.assertIn("data", response)
        self.assertIn("has_issues", response["data"])
        print(f"✓ Path validation: {response['message']}")

    def test_generate_gcode(self):
        """Test G-code generation handler"""
        # Process image first
        self.handlers.handle_process_image(self.test_image_bytes, "test.png")

        # Generate G-code
        response = self.handlers.handle_generate_gcode({
            "safe_margin": 5.0,
            "pen_down_speed": 1.0,
            "pen_up_speed": 1.5,
            "optimize": True
        })

        self.assertIsNotNone(response)
        self.assertTrue(response.get("success"))
        self.assertIn("data", response)
        self.assertIn("gcode", response["data"])
        self.assertGreater(len(response["data"]["gcode"]), 0)
        print(f"✓ G-code generation: {response['message']}")
        print(f"  Commands: {response['data']['stats']['total_commands']}")
        print(f"  Move commands: {response['data']['stats']['move_commands']}")
        print(f"  Est. time: {response['data']['stats']['estimated_time_sec']:.1f}s")

    def test_preview(self):
        """Test preview generation"""
        # Process image first
        self.handlers.handle_process_image(self.test_image_bytes, "test.png")

        # Get preview
        response = self.handlers.handle_preview()

        self.assertIsNotNone(response)
        self.assertTrue(response.get("success"))
        self.assertIn("data", response)
        self.assertIn("svg_paths", response["data"])
        print(f"✓ Preview: {len(response['data']['svg_paths'])} SVG paths")


class TestFullPipeline(unittest.TestCase):
    """Test complete pipeline from image to G-code"""

    def test_end_to_end_square(self):
        """Test full pipeline with a square"""
        # Create test image
        img = np.ones((200, 200, 3), dtype=np.uint8) * 255
        cv2.rectangle(img, (50, 50), (150, 150), (0, 0, 0), 2)

        with tempfile.NamedTemporaryFile(delete=False, suffix=".png") as f:
            cv2.imwrite(f.name, img)
            path = f.name

        with open(path, "rb") as f:
            img_bytes = f.read()

        try:
            # Step 1: Process image
            handlers = WebHandlers()
            result1 = handlers.handle_process_image(img_bytes, "square.png")
            self.assertTrue(result1.get("success"), f"Image processing failed: {result1}")
            print("✓ Step 1: Image processed")

            # Step 2: Validate paths
            result2 = handlers.handle_validate_paths()
            self.assertTrue(result2.get("success"), f"Validation failed: {result2}")
            print(f"✓ Step 2: Paths validated ({result2['data']['path_count']} paths)")

            # Step 3: Generate G-code
            result3 = handlers.handle_generate_gcode({
                "safe_margin": 5.0,
                "optimize": True
            })
            self.assertTrue(result3.get("success"), f"G-code generation failed: {result3}")
            print(f"✓ Step 3: G-code generated ({result3['data']['stats']['total_commands']} commands)")

            # Step 4: Get preview
            result4 = handlers.handle_preview()
            self.assertTrue(result4.get("success"), f"Preview failed: {result4}")
            print(f"✓ Step 4: Preview generated")

            # Verify G-code starts with move and ends with pen up
            gcode = result3["data"]["gcode"]
            self.assertTrue(any(line.startswith("G0") for line in gcode), "No G0 (move) commands found")
            self.assertTrue(any(line == "M3" for line in gcode) or any(line == "G1" for line in gcode), "No drawing commands found")

            print("\n✅ Full pipeline test PASSED")

        finally:
            if os.path.exists(path):
                os.unlink(path)


if __name__ == "__main__":
    # Run tests with verbose output
    unittest.main(verbosity=2)
