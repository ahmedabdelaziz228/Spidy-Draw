#include "kinematics.h"
#include "config.h"
#include <math.h>

long MotorStepPlan::maxAbsSteps() const {
    long maxValue = 0;
    for (int i = 0; i < 4; ++i) {
        long value = labs(steps[i]);
        if (value > maxValue) {
            maxValue = value;
        }
    }
    return maxValue;
}

bool MotorStepPlan::hasMotion() const {
    return maxAbsSteps() > 0;
}

float Kinematics::distanceBetween(float x1, float y1, float x2, float y2) const {
    const float dx = x2 - x1;
    const float dy = y2 - y1;
    return sqrtf(dx * dx + dy * dy);
}

bool Kinematics::isWithinWorkspace(float x, float y) const {
    return x >= 0.0f && x <= WORKSPACE_WIDTH_MM &&
           y >= 0.0f && y <= WORKSPACE_HEIGHT_MM;
}

void Kinematics::calculateDrawingFit(float minX, float minY,
                                     float maxX, float maxY,
                                     float& fitScale,
                                     float& fitOffsetX,
                                     float& fitOffsetY) const {
    float drawingW = maxX - minX;
    float drawingH = maxY - minY;

    float availableW = DRAW_AREA_WIDTH;
    float availableH = DRAW_AREA_HEIGHT;

    if (drawingW <= 0.0f) drawingW = 1.0f;
    if (drawingH <= 0.0f) drawingH = 1.0f;

    float scaleX = availableW / drawingW;
    float scaleY = availableH / drawingH;

    fitScale = (scaleX < scaleY) ? scaleX : scaleY;

    float finalWidth = drawingW * fitScale;
    float finalHeight = drawingH * fitScale;

    fitOffsetX = DRAW_AREA_X + (availableW - finalWidth) / 2.0f - (minX * fitScale);
    fitOffsetY = DRAW_AREA_Y + (availableH - finalHeight) / 2.0f - (minY * fitScale);
}

float Kinematics::cableLengthForMotor(int motorIndex, float x, float y) const {
    const MotorAnchor& anchor = MOTOR_ANCHORS[motorIndex];
    const float dx = x - anchor.x;
    const float dy = y - anchor.y;
    return sqrtf(dx * dx + dy * dy);
}

bool Kinematics::calculateMotorPlan(float currentX, float currentY,
                                    float targetX, float targetY,
                                    MotorStepPlan& plan) const {
    if (!isWithinWorkspace(targetX, targetY)) {
        return false;
    }

    for (int motor = 0; motor < 4; ++motor) {
        const float currentLength = cableLengthForMotor(motor, currentX, currentY);
        const float targetLength = cableLengthForMotor(motor, targetX, targetY);
        const float deltaLength = targetLength - currentLength;

        long motorSteps = lroundf(deltaLength * MOTOR_STEPS_PER_MM[motor]);
        if (labs(motorSteps) < MOTOR_MIN_EFFECTIVE_STEPS) {
            motorSteps = 0;
        }

        plan.steps[motor] = motorSteps * MOTOR_DIRECTION_SIGN[motor];
    }

    return true;
}
