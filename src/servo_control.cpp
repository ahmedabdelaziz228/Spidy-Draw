#include "servo_control.h"
#include "config.h"

ServoController::ServoController() : penIsDown(false) {}

void ServoController::begin() {
    penServo.attach(SERVO_PIN);
    penServo.write(SERVO_UP_ANGLE);
    penIsDown = false;
}

void ServoController::penUp() {
    penServo.write(SERVO_UP_ANGLE);
    penIsDown = false;
}

void ServoController::penDown() {
    penServo.write(SERVO_DOWN_ANGLE);
    penIsDown = true;
}

bool ServoController::isPenDown() const {
    return penIsDown;
}

String ServoController::getStatusString() const {
    return penIsDown ? "DOWN" : "UP";
}