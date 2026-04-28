/* CableRobot.ino - Main Program for 4-Point Cable Drawing Robot
 *
 * Graduation Project - Cable Spider Plotter
 * ESP32 + 4x Stepper Motors + Servo + G-code Support
 *
 * Architecture:
 * - config.h: Hardware pins and constants
 * - motor_control: Stepper motor management
 * - kinematics: Position tracking and calculations
 * - gcode_parser: G-code file parsing
 * - servo_control: Pen up/down control
 * - web_server: WiFi hotspot and web interface
 */
#include <Arduino.h>
#include "config.h"
#include "robot_state.h"
#include "motor_control.h"
#include "kinematics.h"
#include "gcode_parser.h"
#include "servo_control.h"
#include "gcode_executor.h"
#include "web_server.h"

RobotState robotState;
MotorController motors;
Kinematics kinematics;
GCodeParser gcodeParser;
ServoController servoControl;
GCodeExecutor gcodeExecutor(&motors, &kinematics, &servoControl, &robotState);
WebServerManager webServer(&motors, &kinematics, &gcodeParser, &servoControl, &gcodeExecutor, &robotState);

void setup() {
    Serial.begin(115200);
    delay(100);

    motors.begin();
    servoControl.begin();

    // Start from a balanced logical position so the first motion does not
    // assume the gondola is sitting at a corner with extreme cable tension.
    robotState.currentX = START_X_MM;
    robotState.currentY = START_Y_MM;

    if (!webServer.setupWiFi(AP_SSID, AP_PASSWORD)) {
        while (true) {
            delay(1000);
        }
    }

    webServer.begin();
    Serial.println("Robot ready");
}

void loop() {
    webServer.handleClient();
    gcodeExecutor.update();

    if (!motors.isBusy()) {
        robotState.isMoving = false;
    }

    delay(1);
}