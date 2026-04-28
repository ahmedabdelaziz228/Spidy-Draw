#ifndef KINEMATICS_H
#define KINEMATICS_H

#include <Arduino.h>

struct RobotPosition {
    float x = 0.0f;
    float y = 0.0f;
};

struct MotorStepPlan {
    long steps[4] = {0, 0, 0, 0};

    long maxAbsSteps() const;
    bool hasMotion() const;
};

class Kinematics {
public:
    float distanceBetween(float x1, float y1, float x2, float y2) const;

    bool isWithinWorkspace(float x, float y) const;

    void calculateDrawingFit(float minX, float minY,
                             float maxX, float maxY,
                             float& fitScale,
                             float& fitOffsetX,
                             float& fitOffsetY) const;

    bool calculateMotorPlan(float currentX, float currentY,
                            float targetX, float targetY,
                            MotorStepPlan& plan) const;

private:
    float cableLengthForMotor(int motorIndex, float x, float y) const;
};

#endif
