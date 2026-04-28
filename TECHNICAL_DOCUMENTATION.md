# Technical Documentation - Motion Control Project

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                     Web Browser (Client)                     │
├─────────────────────────────────────────────────────────────┤
│  • Image Upload Widget                                       │
│  • Path Preview Canvas (SVG)                                 │
│  • Parameter Controls                                        │
│  • G-Code Viewer / Download                                  │
└────────────┬────────────────────────────────────────────────┘
             │ HTTP(S)
             ▼
┌─────────────────────────────────────────────────────────────┐
│              Web Server (ESP32 on Robot)                     │
├─────────────────────────────────────────────────────────────┤
│  • Image Upload Handler         (POST /api/process-image)    │
│  • G-Code Generator             (POST /api/generate-gcode)   │
│  • Status/Progress              (GET /status, /progress)     │
│  • Manual Control               (GET /move, /servo)          │
└────────────┬────────────────────────────────────────────────┘
             │ Internal (ESP32)
             ▼
┌─────────────────────────────────────────────────────────────┐
│          Firmware Core (C++ on ESP32)                        │
├──────────────┬──────────────────┬──────────────────┬─────────┤
│  GCodeParser │  GCodeExecutor   │  Kinematics      │ Motors  │
│              │                  │                  │         │
│  • Parse G0  │  • Queue mgmt    │  • IK math       │  • M1-4 │
│  • Parse G1  │  • State machine │  • Cable length  │  • Sync │
│  • Parse M3  │  • Transform     │  • Bounds check  │  • Speed│
│  • Parse M5  │  • Pen control   │                  │         │
└──────────────┴──────────────────┴──────────────────┴─────────┘
             │
             ▼
┌─────────────────────────────────────────────────────────────┐
│              Hardware (Cable Robot + Electronics)            │
├─────────────────────────────────────────────────────────────┤
│  • 4 Stepper Motors (M1-M4)                                  │
│  • Servo for Pen Up/Down                                     │
│  • Limit Switches / Encoders (optional)                      │
│  • Power Supply 12V/5V                                       │
└─────────────────────────────────────────────────────────────┘
```

---

## Data Structures

### Point (path_format.h)
```cpp
struct Point {
    float x;  // X coordinate in mm (0-200)
    float y;  // Y coordinate in mm (0-200)
    
    bool isValid() const;
    bool isWithinBounds(float maxX=200, float maxY=200) const;
    float distanceTo(const Point& other) const;
};
```

### VectorPath (path_format.h)
```cpp
struct VectorPath {
    std::vector<Point> points;
    bool closed;                    // polygon vs line
    float lengthMm;                 // calculated path length
    int originalPointCount;         // before simplification
    bool isSimplified;              // DP applied?
    float simplifyTolerance;        // DP epsilon used
    
    bool isValid() const;
    bool isTrivial(float minLengthMm=0.5f) const;
    void calculateLength();
    void getBounds(float& minX, float& minY, float& maxX, float& maxY) const;
};
```

### PathExtractionResult (path_format.h)
```cpp
struct PathExtractionResult {
    std::vector<VectorPath> paths;
    float totalLengthMm;
    int totalPoints;
    float minX, minY, maxX, maxY;
    bool hasClosedPaths;
    bool hasOpenPaths;
    const char* sourceImage;
    const char* processingNotes;
    
    bool isValid() const;
    void calculateStats();
};
```

### GCodeCommand (gcode_parser.h)
```cpp
struct GCodeCommand {
    char type;          // 'G' or 'M'
    int code;           // 0,1,3,5...
    float x, y;         // coordinates
    bool hasX, hasY;    // which coords present?
    bool valid;         // successfully parsed?
    String originalLine;
};
```

---

## API Endpoints

### Image Processing

**POST /api/process-image**
```
Request:
  multipart/form-data
  image: <binary PNG/JPG>

Response:
  {
    "success": true,
    "data": {
      "paths": [...],
      "total_length_mm": 150.5,
      "total_points": 1200,
      "bounds": {...},
      "path_count": 5
    }
  }
```

### G-Code Generation

**POST /api/generate-gcode**
```
Request:
  application/json
  {
    "safe_margin": 5.0,
    "pen_down_speed": 1.0,
    "pen_up_speed": 1.5,
    "optimize": true
  }

Response:
  {
    "success": true,
    "data": {
      "gcode": ["G0 X10 Y10", "M3", "G1 X50 Y50", ...],
      "stats": {
        "total_commands": 150,
        "move_commands": 100,
        "estimated_time_sec": 45
      }
    }
  }
```

### Validation

**POST /api/validate-paths**
```
Response:
  {
    "success": true,
    "data": {
      "path_count": 5,
      "total_points": 1200,
      "has_issues": false,
      "issues": []
    }
  }
```

### Preview

**GET /api/preview**
```
Response:
  {
    "success": true,
    "data": {
      "svg_paths": [
        {
          "d": "M10,10L50,50L100,100",
          "id": "path_0",
          "points": 3,
          "closed": false
        }
      ],
      "bounds": {
        "minX": 0, "minY": 0,
        "maxX": 200, "maxY": 200
      }
    }
  }
```

---

## Processing Pipeline

### Image Processing (image_processor.py)

```
1. Load Image
   └─ Verify file format and dimensions

2. Preprocess
   ├─ Resize to target (200×200)
   ├─ Grayscale conversion
   ├─ Gaussian blur (noise reduction)
   └─ Binary threshold

3. Contour Detection
   └─ OpenCV findContours (external contours)

4. Contour Processing (per contour)
   ├─ Convert to Point list
   ├─ Remove duplicates (distance < 0.1mm)
   ├─ Apply Douglas-Peucker simplification
   ├─ Calculate length
   └─ Validate bounds

5. Path Filtering
   ├─ Remove trivial paths (< 0.5mm)
   ├─ Remove invalid points
   └─ Calculate statistics

6. Output
   └─ PathExtractionResult with all metadata
```

### G-Code Generation (gcode_validator.h + web_handlers.py)

```
1. Validate Paths
   ├─ Check bounds
   ├─ Remove NaN/inf
   ├─ Remove duplicates
   └─ Clean output

2. Calculate Fit Transform
   ├─ Find bounds (min/max X, Y)
   ├─ Calculate scale to fit workspace
   └─ Calculate offsets for centering

3. Generate G-Code
   ├─ Pen up move to first point (G0)
   ├─ For each point:
   │   ├─ Pen down if needed (M3)
   │   ├─ Move to point (G1)
   │   └─ Pen up if path ends (M5)
   └─ Final pen up (M5)

4. Optimize (optional)
   ├─ Remove redundant pen commands
   ├─ Reorder paths to minimize jumps
   └─ Compress coordinates

5. Output
   └─ Array of G-code command strings
```

### Execution (gcode_executor.cpp)

```
1. Queue Setup
   ├─ Parse all commands
   ├─ Calculate fit transform
   └─ Prepare state

2. Execution Loop
   ├─ State machine with states:
   │   ├─ IDLE
   │   ├─ STARTING
   │   ├─ SETTING_PEN (M3/M5)
   │   ├─ WAITING_SERVO (settle time)
   │   ├─ STARTING_MOVE (calc IK)
   │   ├─ WAITING_MOVE (motor feedback)
   │   └─ FINISHED
   ├─ For each command:
   │   ├─ Apply transform (scale + offset)
   │   ├─ Check workspace bounds
   │   ├─ Calculate motor steps (inverse kinematics)
   │   └─ Execute synchronized stepping
   └─ Update robot state

3. Completion
   ├─ Pen up
   ├─ Clear queue
   └─ Return to idle
```

---

## Configuration

### Hardware Bounds (config.h)
```cpp
DRAW_AREA_X = 20.0f;        // Safe area start X
DRAW_AREA_Y = 20.0f;        // Safe area start Y
DRAW_AREA_WIDTH = 160.0f;   // Safe drawing width
DRAW_AREA_HEIGHT = 160.0f;  // Safe drawing height
WORKSPACE_WIDTH_MM = 200.0f;  // Total workspace
WORKSPACE_HEIGHT_MM = 200.0f;
WORKSPACE_MARGIN_MM = 10.0f; // Safety margin
```

### Processing Parameters
See `preset_modes.py` for preset configurations

### Servo Settings (config.h)
```cpp
SERVO_UP_ANGLE = 25;        // Pen up angle
SERVO_DOWN_ANGLE = 10;      // Pen down angle
SERVO_SETTLE_MS = 300;      // Wait time after move
```

---

## Coordinate System

```
      0 ─────────── 200 (X)
      │
    0 ├─────────┐
      │ SAFE    │
  160 │ AREA    │ 160×160
      │         │
    │ 
  200 └─────────┘

Origin: Top-left (0,0)
X-axis: Left to right (positive →)
Y-axis: Top to bottom (positive ↓)
Scale: 1 unit = 1 mm
```

---

## Scaling Algorithm

```
Given: Drawing bounds (minX, minY, maxX, maxY)
Output: scale, offsetX, offsetY

1. Calculate drawing size
   drawingW = maxX - minX
   drawingH = maxY - minY

2. Calculate available space
   availableW = WORKSPACE_WIDTH - 2*SAFE_MARGIN
   availableH = WORKSPACE_HEIGHT - 2*SAFE_MARGIN

3. Calculate scale to fit (maintain aspect ratio)
   scaleX = availableW / drawingW
   scaleY = availableH / drawingH
   scale = min(scaleX, scaleY)
   scale = clamp(scale, 0.1, 10.0)

4. Calculate offsets for centering
   scaledW = drawingW * scale
   scaledH = drawingH * scale
   offsetX = SAFE_MARGIN + (availableW - scaledW) / 2 - minX * scale
   offsetY = SAFE_MARGIN + (availableH - scaledH) / 2 - minY * scale
```

---

## Inverse Kinematics (kinematics.cpp)

For each target point (X, Y):
```
1. Calculate cable length needed per motor
   For each motor i:
     currentLength = distance(motorAnchor[i], currentPos)
     targetLength = distance(motorAnchor[i], targetPos)
     deltaLength = targetLength - currentLength

2. Convert length to steps
   motorSteps[i] = deltaLength * MOTOR_STEPS_PER_MM[i]
   motorSteps[i] = motorSteps[i] * MOTOR_DIRECTION_SIGN[i]

3. Execute synchronized stepping
   All 4 motors move simultaneously, controlled by max step count
```

---

## Performance Optimization

### Point Simplification (Douglas-Peucker)
- Reduces points by 30-70% typically
- Threshold: 0.5mm default, 0.1-1.5mm range
- Time: O(n²) but acceptable for < 5000 points

### Path Ordering (path_optimizer.py)
- Nearest-neighbor greedy algorithm
- Reduces pen-up travel by 30-50%
- Time: O(n²) for n paths

### G-Code Optimization (gcode_exporter.py)
- Removes redundant M3/M5 commands
- Merges collinear move commands
- Saves 5-10% command count

---

## Testing Strategy

### Unit Tests (test_integration.py)
- Point distance calculations
- Path validation
- Bounds calculation
- Scaling math
- Simplification algorithm

### Integration Tests
- Image → paths (5 test images)
- Paths → G-code
- G-code execution (simulation)
- Full pipeline end-to-end

### Hardware Tests
- Pen up/down reliability
- Motor synchronization
- Workspace bounds enforcement
- Execution consistency

---

## Debugging

### Serial Output (115200 baud)
- `[GCODE] Bounds: (X, Y) to (X, Y)`
- `[EXEC] Transform: scale=X, offset=(X,Y)`
- `[MOTOR] Step plan for point X,Y`
- `[ERROR] Out of workspace`

### Web Console Logging
- Image processing steps
- G-code generation details
- Transform calculations
- Statistics updates

### Logs/Export
- Export paths as JSON (debug format)
- Export G-code with comments
- Export SVG for visualization
- Export statistics CSV

---

## Known Limitations

1. **Point Density**: Auto-limited to ~5000 points to prevent memory overflow
2. **Execution Time**: Limited to ~10 minutes max draw time
3. **Precision**: ±1mm typical due to cable elasticity
4. **Resolution**: Pixel-to-mm conversion at 200mm scale
5. **Workspace**: 200×200mm hard limit (160×160mm safe)

---

## Future Improvements

1. Path merging (join nearby path endpoints)
2. Curvature-based simplification (preserve curves better)
3. Pressure-based pen control
4. Real-time motion feedback
5. Multi-layer drawing (lift pen between layers)
6. Path smoothing with Bézier curves

---

## References

- Douglas-Peucker Algorithm: Simplify polylines
- Inverse Kinematics: Cable tension calculation
- G-Code Standard: NIST RS274/NGC
- Arduino/ESP32 APIs: PlatformIO documentation

---

End of Technical Documentation
