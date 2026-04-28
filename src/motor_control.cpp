#include "motor_control.h"
#include "config.h"

MotorController::MotorController()
    : busy(false),
      stopRequested(false),
      currentSpeedUs(NORMAL_SPEED_US),
      lastStepMicros(0),
      majorAxisSteps(0),
      majorAxisProgress(0) {
}

void MotorController::begin() {
    for (int motor = 0; motor < 4; motor++) {
        for (int pin = 0; pin < 4; pin++) {
            pinMode(motorPins[motor][pin], OUTPUT);
            digitalWrite(motorPins[motor][pin], LOW);
        }
    }
}

void MotorController::applyHalfStep(int motor, int step) {
    if (motor < 0 || motor > 3) return;

    for (int i = 0; i < 4; i++) {
        digitalWrite(motorPins[motor][i], halfStep[step][i]);
    }
}

void MotorController::stepMotor(int motor, int direction) {
    if (direction == 0) {
        return;
    }

    stepIndex[motor] += direction;
    if (stepIndex[motor] < 0) stepIndex[motor] = HALF_STEP_SEQUENCE - 1;
    if (stepIndex[motor] >= HALF_STEP_SEQUENCE) stepIndex[motor] = 0;

    applyHalfStep(motor, stepIndex[motor]);
}

bool MotorController::startMovePlan(const MotorStepPlan& plan, bool fastMode) {
    if (busy || !plan.hasMotion()) {
        return false;
    }

    activePlan = plan;
    stopRequested = false;
    currentSpeedUs = fastMode ? FAST_SPEED_US : NORMAL_SPEED_US;
    lastStepMicros = micros();

    for (int i = 0; i < 4; ++i) {
        absTargetSteps[i] = labs(activePlan.steps[i]);
        executedSteps[i] = 0;
        accumulators[i] = 0;
    }

    majorAxisSteps = plan.maxAbsSteps();
    majorAxisProgress = 0;
    busy = majorAxisSteps > 0;
    return busy;
}

bool MotorController::startManualMove(float deltaXmm, float deltaYmm, const Kinematics& kinematics,
                                      float currentX, float currentY, bool fastMode) {
    MotorStepPlan plan;
    const float targetX = currentX + deltaXmm;
    const float targetY = currentY + deltaYmm;

    if (!kinematics.calculateMotorPlan(currentX, currentY, targetX, targetY, plan)) {
        return false;
    }

    return startMovePlan(plan, fastMode);
}

void MotorController::update() {
    if (!busy) return;

    if (stopRequested) {
        stopAllMotors();
        busy = false;
        return;
    }

    if (majorAxisProgress >= majorAxisSteps) {
        stopAllMotors();
        busy = false;
        return;
    }

    unsigned long now = micros();
    if ((now - lastStepMicros) < static_cast<unsigned long>(currentSpeedUs)) {
        return;
    }
    lastStepMicros = now;

    ++majorAxisProgress;
    for (int motor = 0; motor < 4; ++motor) {
        if (absTargetSteps[motor] == 0) {
            continue;
        }

        accumulators[motor] += absTargetSteps[motor];
        if (accumulators[motor] >= majorAxisSteps && executedSteps[motor] < absTargetSteps[motor]) {
            accumulators[motor] -= majorAxisSteps;
            const int direction = (activePlan.steps[motor] > 0) ? 1 : -1;
            stepMotor(motor, direction);
            ++executedSteps[motor];
        }
    }

    if (majorAxisProgress >= majorAxisSteps) {
        stopAllMotors();
        busy = false;
    }
}

bool MotorController::isBusy() const {
    return busy;
}

void MotorController::requestStop() {
    stopRequested = true;
}

void MotorController::stopAllMotors() {
    for (int motor = 0; motor < 4; motor++) {
        for (int pin = 0; pin < 4; pin++) {
            digitalWrite(motorPins[motor][pin], LOW);
        }
    }
}
