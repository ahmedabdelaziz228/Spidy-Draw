# Spidy-Draw

> Portable ESP32 cable-driven drawing robot with an end-to-end **image → paths → G-code → execution** workflow.

![Status](https://img.shields.io/badge/status-prototype-blue)
![Platform](https://img.shields.io/badge/platform-ESP32-green)
![Motion](https://img.shields.io/badge/motion-cable--driven-orange)
![Pipeline](https://img.shields.io/badge/pipeline-image→paths→G--code-informational)

Spidy-Draw is a portable robotic drawing system that converts uploaded images into cleaned vector-like paths, generates validated G-code, and executes the drawing on an ESP32-controlled robot using synchronized multi-motor motion.

The project combines:

- image processing and contour extraction
- path cleanup, simplification, and ordering
- G-code validation and workspace fitting
- firmware-side execution on ESP32

---

## Table of Contents

- [Overview](#overview)
- [Project Status](#project-status)
- [Core Features](#core-features)
- [System Architecture](#system-architecture)
- [How It Works](#how-it-works)
- [Project Structure](#project-structure)
- [Coordinate System](#coordinate-system)
- [Data Model](#data-model)
- [API Endpoints](#api-endpoints)
- [Quick Start](#quick-start)
- [Firmware Workflow](#firmware-workflow)
- [Testing](#testing)
- [Technical Highlights](#technical-highlights)
- [Known Limitations](#known-limitations)
- [Roadmap](#roadmap)
- [Documentation](#documentation)
- [Demo](#demo)
- [Team](#team)
- [License](#license)

---

## Overview

This repository implements a complete **image → paths → validated G-code → drawing execution** pipeline.

The architecture is intentionally split into two sides:

- **processing side** on laptop / browser
- **execution side** on ESP32 firmware

This makes the system lighter on the microcontroller and keeps the image-processing pipeline flexible and easier to improve.

The project integrates:

- image vectorization and cleanup
- path optimization
- G-code generation and validation
- firmware-side motion execution
- web-based UI and upload flow

---

## Project Status

**Status:** Prototype / active development  
**Target:** Portable cable-driven drawing robot  
**Execution Platform:** ESP32  
**Motion Type:** 4-motor synchronized cable-driven motion  
**Processing Pipeline:** image → contour extraction → simplified paths → validated G-code

---

## Core Features

### Image Processing
- Real JPG / PNG image loading
- Contour extraction
- Noise reduction
- Path simplification
- Duplicate point removal
- Path filtering by area and path length
- Pixel-to-workspace conversion
- Multiple preset modes

### Path Optimization
- Path cleanup
- Path ordering optimization
- Reduced pen-up travel
- Better drawing flow between shapes

### G-code Generation
- Path validation before conversion
- Dynamic workspace fitting
- Bounds checking
- Safe margin handling
- Optimized command output
- ESP32-compatible commands:
  - `G0`
  - `G1`
  - `M3`
  - `M5`

### Firmware / Execution
- Queue-based G-code execution
- Pen up / pen down control
- Transform and scaling before execution
- Motion planning using kinematics
- Synchronized multi-motor stepping
- Status and progress reporting

### UI / Integration
- Image upload
- Path preview
- Parameter controls
- G-code generation
- Upload-to-device workflow

---

## System Architecture

The project is organized into 4 main logical layers:

### 1. Image Processing Layer
Responsible for:
- loading images
- extracting contours
- simplifying paths
- removing invalid points
- preparing clean drawing data

### 2. G-code Generation Layer
Responsible for:
- validating paths
- fitting paths into workspace
- generating safe and optimized G-code
- exporting debug information

### 3. Firmware Execution Layer
Runs on ESP32 and handles:
- G-code parsing
- execution queue
- kinematics
- motor control
- servo control
- robot state updates

### 4. Web / Control Layer
Responsible for:
- image upload
- preview
- parameter tuning
- generation workflow
- sending G-code to the robot

---

## How It Works

### End-to-End Flow

1. Upload an image
2. Extract contours and convert them into vector-like paths
3. Simplify and clean the paths
4. Reorder paths to reduce pen-up travel
5. Validate and generate G-code
6. Upload G-code to ESP32
7. Execute drawing with synchronized motor motion

### Typical User Flow

1. Run the local processing server
2. Open the browser UI
3. Upload an image
4. Preview extracted paths
5. Generate G-code
6. Send G-code to the ESP32
7. Execute drawing

### Important Note

The firmware side is intended for **motion execution**, not heavy image processing.

---

## Project Structure

```text
fixed_project/
├── .gitignore
├── COMPLETION_CHECKLIST.txt
├── FINAL_INTEGRATED_README.md
├── FINAL_REPORT.md
├── FINAL_VERIFICATION.txt
├── IMPLEMENTATION_SUMMARY.md
├── MOHAMED_COMPLETED_WORK.md
├── MOTION_CONTROL_NOTES.txt
├── PROJECT_DELIVERY_CHECKLIST.md
├── PolarGraphDescription.md
├── README_FINAL_DELIVERY.md
├── START_HERE.txt
├── TECHNICAL_DOCUMENTATION.md
├── USER_GUIDE.md
├── bridge_server.py
├── gcode_exporter.py
├── image_processor.py
├── path_optimizer.py
├── platformio.ini
├── preset_modes.py
├── requirements.txt
├── test_final_integration.py
├── test_integration.py
├── test_procedures.py
├── ui_components.html
├── web_handlers.py
├── .idea/
├── .pio/
├── .vscode/
├── include/
│   ├── README
│   ├── config.h
│   ├── gcode_executor.h
│   ├── gcode_parser.h
│   ├── gcode_validator.h
│   ├── kinematics.h
│   ├── motor_control.h
│   ├── robot_state.h
│   ├── servo_control.h
│   └── web_server.h
├── lib/
│   ├── README
│   └── library.json
├── PolarGraphPics/
│   ├── PolarGraph_01.jpg
│   ├── PolarGraph_02.jpg
│   └── PolarGraph_03.jpg
├── src/
│   ├── gcode_executor.cpp
│   ├── gcode_parser.cpp
│   ├── kinematics.cpp
│   ├── main.cpp
│   ├── motor_control.cpp
│   ├── path_format.h
│   ├── servo_control.cpp
│   ├── web_server.cpp
│   └── build/
├── test/
│   └── README
├── web/
│   └── index.html
└── __pycache__/
