# User Guide - Motion Control Drawing Robot

## Quick Start

### Step 1: Access the Web Interface
1. Turn on the robot
2. Connect to WiFi hotspot: **CableRobot_Hotspot**
3. Password: **robot123**
4. Open browser: `http://192.168.4.1`

### Step 2: Upload an Image
1. Click **"Upload & Process"**
2. Select an image file (PNG, JPG, SVG)
3. Wait for processing (usually 1-2 seconds)
4. See the extracted paths in the preview

### Step 3: Configure Settings (Optional)
1. **Select Mode:**
   - **Logo Mode**: Clean, simplified drawings
   - **Signature Mode**: Preserve fine details
   - **Fine-Art Mode**: Maximum detail preservation
   - **Outline Mode**: Simple outlines only

2. **Fine-Tune Parameters:**
   - **Simplification**: How much to simplify curves (0.1-5.0mm)
   - **Min Path Length**: Skip paths shorter than this
   - **Min Point Distance**: Minimum spacing between points

### Step 4: Generate G-Code
1. Click **"Convert to G-Code"**
2. Review the statistics (command count, draw time)
3. Click **"Download G-Code"** to save and review

### Step 5: Execute
1. Click **"Upload to Robot"** on the web page
2. Robot loads the G-code
3. Click **"Execute"** to start drawing
4. Monitor progress on the status display

---

## Preset Modes Explained

### Logo Mode
**Best for:** Logos, icons, simple graphics
- **Simplification:** High (1.0mm tolerance)
- **Noise Reduction:** Heavy blurring
- **Result:** Clean lines, fewer points, fast execution

### Signature Mode  
**Best for:** Handwritten text, signatures
- **Simplification:** Medium (0.3mm tolerance)
- **Point Density:** High
- **Result:** Preserves detail and curves

### Fine-Art Mode
**Best for:** Detailed drawings, photographs
- **Simplification:** Minimal (0.1mm tolerance)
- **Max Points:** No limit
- **Result:** Maximum detail, slower execution

### Outline Mode
**Best for:** Outline extraction, contour drawings
- **Simplification:** Aggressive (1.5mm)
- **Min Path Length:** 2.0mm (filters tiny paths)
- **Result:** Simple, bold outlines

---

## Statistics Explained

**Paths Detected:** Number of separate drawing paths
**Total Points:** Sum of all coordinate points before optimization
**Total Length:** Total drawing distance in millimeters
**Point Reduction:** Percentage of points simplified away
**Est. Draw Time:** Approximate execution time

---

## Troubleshooting

### "No image processed yet"
- Click "Upload & Process" first
- Check that file is valid PNG/JPG

### "No valid paths extracted"
- Image may be too light or have no contrasting edges
- Try adjusting threshold in parameters
- Ensure image has clear black lines on white background

### "Drawing goes out of bounds"
- ⚠️ This is prevented by auto-scaling
- If warning appears, reduce image size or use simplified mode

### "Too many commands"
- Use Logo Mode or Outline Mode for simplification
- Increase simplification tolerance

### Robot doesn't execute
- Check WiFi connection
- Verify G-code was uploaded successfully
- Try clicking "Execute" again

---

## Tips for Best Results

### Image Preparation
1. **Use high contrast** (black on white)
2. **Avoid thin lines** (< 2px may disappear)
3. **Remove noise** - Use an image editor first if needed
4. **Optimal size** - 200×200 px or larger

### Mode Selection
1. **Logos** → Use "Logo Mode"
2. **Handwriting** → Use "Signature Mode"  
3. **Detailed art** → Use "Fine-Art Mode"
4. **Simple shapes** → Use "Outline Mode"

### Performance
- **Logos:** ~30 seconds
- **Signatures:** ~60 seconds
- **Complex drawings:** 2-5 minutes
- **Very detailed art:** 5-10+ minutes

### Safety
- ✓ Always preview before executing
- ✓ Never exceed 3000 commands (auto-limited)
- ✓ Check drawing area is clear
- ✓ Stop immediately if issues occur
- ✓ Keep cables taut and properly tensioned

---

## Manual Control

### Joystick Mode (For Testing)
1. Click **"Manual Control"**
2. Use angle/distance to move pen
3. Use **"Pen Up/Down"** buttons to test servo

### Home Position
- Click **"Home"** to reset position to (0,0)
- Useful before starting new drawings

### Stop Execution
- Click **"Stop"** at any time
- Pen automatically raises
- Resume from last position (may lose sync)

---

## Advanced: Custom Parameters

If presets don't work, customize parameters:

| Parameter | Range | Effect |
|-----------|-------|--------|
| Simplification | 0.1-5.0 | Higher = fewer points, faster |
| Min Path Length | 0.1-10.0 | Filters out tiny paths |
| Min Point Distance | 0.01-1.0 | Minimum space between points |
| Blur Kernel | 3-9 | Noise reduction strength |
| Threshold | 50-200 | Edge detection sensitivity |

Start with a preset and adjust from there.

---

## Limitations

- **Max drawing area:** 160×160 mm (safe zone)
- **Max commands:** 3000 G-code lines
- **Max file size:** 200 KB
- **Point density:** Auto-limited to ~5000 points
- **Execution time:** Typically < 10 minutes

---

## Support

For issues or questions:
1. Check troubleshooting section above
2. Review system status on web interface
3. Try a simple test image (square, circle)
4. Check ESP32 serial output for debugging info

---

## Glossary

**G-Code:** Standard CNC programming language (G0=move, G1=draw, M3=pen down, M5=pen up)

**Simplification:** Algorithm to reduce point count while preserving shape

**Path Ordering:** Optimization to minimize pen-up travel between shapes

**Contour:** Outline of an object in an image

**Workspace:** Physical area where robot can draw (200×200mm)

**Vector:** Mathematical representation of a line or curve

---

## Safety Checklist

Before each use:
- [ ] Cables are tight and untangled
- [ ] Pen is installed and functional
- [ ] Drawing surface is clean and level
- [ ] No obstacles in workspace
- [ ] Test pen up/down motion
- [ ] Preview looks correct before executing
- [ ] Can reach STOP button easily

---

End of User Guide
