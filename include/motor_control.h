#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <Arduino.h>
#include "kinematics.h"

class MotorController {
public:
    MotorController();

    void begin();

    bool startMovePlan(const MotorStepPlan& plan, bool fastMode);
    bool startManualMove(float deltaXmm, float deltaYmm, const Kinematics& kinematics,
                         float currentX, float currentY, bool fastMode);
    void update();

    bool isBusy() const;
    void requestStop();
    void stopAllMotors();

private:
    static const int HALF_STEP_SEQUENCE = 8;

    const uint8_t halfStep[8][4] = {
        {1,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,1,1,0},
        {0,0,1,0}, {0,0,1,1}, {0,0,0,1}, {1,0,0,1}
    };

    const int motorPins[4][4] = {
        {19,18,5,17},
        {16,4,0,2},
        {13,12,14,27},
        {26,25,33,32}
    };

    int stepIndex[4] = {0, 0, 0, 0};

    bool busy;
    bool stopRequested;
    int currentSpeedUs;
    unsigned long lastStepMicros;

    MotorStepPlan activePlan;
    long absTargetSteps[4];
    long executedSteps[4];
    long accumulators[4];
    long majorAxisSteps;
    long majorAxisProgress;

    void applyHalfStep(int motor, int step);
    void stepMotor(int motor, int direction);
};

#endif
