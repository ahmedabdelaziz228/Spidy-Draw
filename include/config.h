/* config.h - Hardware Configuration for Integrated Drawing Robot */
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <cstddef>

// WiFi
static const char* AP_SSID = "CableRobot_Hotspot";
static const char* AP_PASSWORD = "robot123";

// Servo
static const int SERVO_PIN = 15;
static const int SERVO_UP_ANGLE = 25;
static const int SERVO_DOWN_ANGLE = 10;
static const unsigned long SERVO_SETTLE_MS = 300;

// Safe drawing area on the paper / desk
static constexpr float DRAW_AREA_X = 20.0f;
static constexpr float DRAW_AREA_Y = 20.0f;
static constexpr float DRAW_AREA_WIDTH = 160.0f;
static constexpr float DRAW_AREA_HEIGHT = 160.0f;

// Workspace exposed to G-code after fitting
static constexpr float WORKSPACE_WIDTH_MM = 200.0f;
static constexpr float WORKSPACE_HEIGHT_MM = 200.0f;
static constexpr float WORKSPACE_MARGIN_MM = 10.0f;
static constexpr float MIN_MOVE_MM = 0.5f;

// Safer logical start/home position: keep the gondola near the center so
// cable tension stays more balanced instead of assuming a corner start.
static constexpr float START_X_MM = WORKSPACE_WIDTH_MM / 2.0f;
static constexpr float START_Y_MM = WORKSPACE_HEIGHT_MM / 2.0f;

// Motion profile
static const int NORMAL_SPEED_US = 2500;
static const int FAST_SPEED_US = 1800;
static constexpr float MANUAL_MOVE_MM = 5.0f;

// Upload
static const size_t MAX_GCODE_COMMANDS = 3000;
static const size_t MAX_UPLOAD_BYTES = 200000;

// ====== Motor Pins ======
// Motor order used everywhere: M1, M2, M3, M4
static constexpr int M1_IN1 = 19;
static constexpr int M1_IN2 = 18;
static constexpr int M1_IN3 = 5;
static constexpr int M1_IN4 = 17;

static constexpr int M2_IN1 = 16;
static constexpr int M2_IN2 = 4;
static constexpr int M2_IN3 = 0;
static constexpr int M2_IN4 = 2;

static constexpr int M3_IN1 = 13;
static constexpr int M3_IN2 = 12;
static constexpr int M3_IN3 = 14;
static constexpr int M3_IN4 = 27;

static constexpr int M4_IN1 = 26;
static constexpr int M4_IN2 = 25;
static constexpr int M4_IN3 = 33;
static constexpr int M4_IN4 = 32;

// ====== Integrated-device inverse kinematics model ======
// IMPORTANT:
// These anchor positions are placeholders for the REAL physical locations of the
// four motor/string anchor points on your compact integrated device.
// Measure them on the real machine and update them during calibration.
struct MotorAnchor {
    float x;
    float y;
};

// Default anchor model: four anchor points around the usable motion region.
static constexpr MotorAnchor MOTOR_ANCHORS[4] = {
    {0.0f, 0.0f},                               // M1
    {WORKSPACE_WIDTH_MM, 0.0f},                 // M2
    {0.0f, WORKSPACE_HEIGHT_MM},                // M3
    {WORKSPACE_WIDTH_MM, WORKSPACE_HEIGHT_MM}   // M4
};

// Per-motor calibration. Signs may need tuning on the real device.
static constexpr float MOTOR_STEPS_PER_MM[4] = {85.0f, 85.0f, 85.0f, 85.0f};
static constexpr int MOTOR_DIRECTION_SIGN[4] = {1, 1, 1, 1};
static constexpr int MOTOR_MIN_EFFECTIVE_STEPS = 2;

#endif // CONFIG_H
