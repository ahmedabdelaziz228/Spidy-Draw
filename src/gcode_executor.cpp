// Use relative includes to ensure headers are found when compiling from src/
#include "../include/gcode_executor.h"
#include "../include/gcode_validator.h"
#include "../include/config.h"

GCodeExecutor::GCodeExecutor(MotorController *motors,
                             Kinematics *kin,
                             ServoController *servo,
                             RobotState *state)
    : motorController(motors),
      kinematics(kin),
      servoController(servo),
      robotState(state),
      execState(ExecState::IDLE),
      waitUntilMs(0),
      currentIndex(0),
      fitScale(1.0f),
      fitOffsetX(0.0f),
      fitOffsetY(0.0f),
      rawCurrentX(0.0f),
      rawCurrentY(0.0f) {

    // Initialize validator with proper config
    GCodeGenerationConfig cfg;
    cfg.workspace_x_min = 0.0f;
    cfg.workspace_x_max = WORKSPACE_WIDTH_MM;
    cfg.workspace_y_min = 0.0f;
    cfg.workspace_y_max = WORKSPACE_HEIGHT_MM;
    cfg.safe_margin = WORKSPACE_MARGIN_MM;
    cfg.min_segment_length = MIN_MOVE_MM;
    validator.reset(new GCodeValidator(cfg));
}

void GCodeExecutor::setQueue(const std::vector<GCodeCommand>& commands) {
    gcodeQueue = commands;
    currentIndex = 0;
    robotState->currentCommand = 0;
    robotState->totalCommands = gcodeQueue.size();
}

void GCodeExecutor::clearQueue() {
    gcodeQueue.clear();
    currentIndex = 0;
    rawCurrentX = 0.0f;
    rawCurrentY = 0.0f;
    execState = ExecState::IDLE;
    robotState->currentCommand = 0;
    robotState->totalCommands = 0;
    robotState->isExecuting = false;
    robotState->isMoving = false;
    robotState->stopRequested = false;
}

bool GCodeExecutor::start() {
    if (gcodeQueue.empty() || robotState->isExecuting) {
        return false;
    }

    prepareQueueFit();

    currentIndex = 0;
    rawCurrentX = 0.0f;
    rawCurrentY = 0.0f; 
    robotState->currentCommand = 0;
    robotState->totalCommands = gcodeQueue.size();
    robotState->isExecuting = true;
    robotState->stopRequested = false;

    execState = ExecState::STARTING;
    return true;
}

void GCodeExecutor::stop() {
    robotState->stopRequested = true;
    motorController->requestStop();
}

bool GCodeExecutor::isExecuting() const {
    return robotState->isExecuting;
}

bool GCodeExecutor::hasQueue() const {
    return !gcodeQueue.empty();
}

size_t GCodeExecutor::getCurrentIndex() const {
    return currentIndex;
}

size_t GCodeExecutor::getTotalCommands() const {
    return gcodeQueue.size();
}

void GCodeExecutor::prepareQueueFit() {
    if (gcodeQueue.empty()) {
        fitScale = 1.0f;
        fitOffsetX = 0.0f;
        fitOffsetY = 0.0f;
        return;
    }

    float minX = 999999.0f;
    float minY = 999999.0f;
    float maxX = -999999.0f;
    float maxY = -999999.0f;
    bool foundPoint = false;

    // Scan all G-commands for bounds
    for (const auto& cmd : gcodeQueue) {
        if (!cmd.valid || cmd.type != 'G') continue;

        if (cmd.hasX) {
            if (cmd.x < minX) minX = cmd.x;
            if (cmd.x > maxX) maxX = cmd.x;
            foundPoint = true;
        }

        if (cmd.hasY) {
            if (cmd.y < minY) minY = cmd.y;
            if (cmd.y > maxY) maxY = cmd.y;
            foundPoint = true;
        }
    }

    if (!foundPoint) {
        fitScale = 1.0f;
        fitOffsetX = 0.0f;
        fitOffsetY = 0.0f;
        return;
    }

    // FIX: Use proper validator to calculate transform
    // (was: kinematics->calculateDrawingFit(minX, minY, maxX, maxY, fitScale, fitOffsetX, fitOffsetY);)
    validator->calculateFitTransform(minX, minY, maxX, maxY, fitScale, fitOffsetX, fitOffsetY);

    robotState->fitScale = fitScale;
    robotState->fitOffsetX = fitOffsetX;
    robotState->fitOffsetY = fitOffsetY;

    Serial.printf("G-Code bounds: (%.2f, %.2f) to (%.2f, %.2f)\n", minX, minY, maxX, maxY);
    Serial.printf("Applied transform: scale=%.3f, offset=(%.2f, %.2f)\n", fitScale, fitOffsetX, fitOffsetY);
}

void GCodeExecutor::executeCurrentCommand() {
    if (currentIndex >= gcodeQueue.size()) {
        execState = ExecState::FINISHED;
        return;
    }

    const GCodeCommand& cmd = gcodeQueue[currentIndex];

    if (!cmd.valid) {
        currentIndex++;
        robotState->currentCommand = currentIndex;
        return;
    }

    if (cmd.type == 'M') {
        if (cmd.code == 3) {
            servoController->penDown();
            robotState->penDown = true;
            waitUntilMs = millis() + SERVO_SETTLE_MS;
            execState = ExecState::WAITING_SERVO;
            return;
        }

        if (cmd.code == 5) {
            servoController->penUp();
            robotState->penDown = false;
            waitUntilMs = millis() + SERVO_SETTLE_MS;
            execState = ExecState::WAITING_SERVO;
            return;
        }

        currentIndex++;
        robotState->currentCommand = currentIndex;
        return;
    }

    if (cmd.type == 'G') {
        bool shouldPenDown = (cmd.code == 1);

        if (robotState->penDown != shouldPenDown) {
            if (shouldPenDown) servoController->penDown();
            else servoController->penUp();

            robotState->penDown = shouldPenDown;
            waitUntilMs = millis() + SERVO_SETTLE_MS;
            execState = ExecState::WAITING_SERVO;
            return;
        }

        execState = ExecState::STARTING_MOVE;
        return;
    }

    currentIndex++;
    robotState->currentCommand = currentIndex;
}

void GCodeExecutor::finishExecution() {
    motorController->stopAllMotors();
    servoController->penUp();
    robotState->penDown = false;
    robotState->isExecuting = false;
    robotState->isMoving = false;
    robotState->stopRequested = false;
    execState = ExecState::IDLE;
}

void GCodeExecutor::update() {
    motorController->update();

    if (!robotState->isExecuting) {
        return;
    }

    if (robotState->stopRequested) {
        finishExecution();
        return;
    }

    switch (execState) {
        case ExecState::STARTING:
            execState = ExecState::SETTING_PEN;
            break;

        case ExecState::SETTING_PEN:
            executeCurrentCommand();
            break;

        case ExecState::WAITING_SERVO:
            if (millis() >= waitUntilMs) {
                const GCodeCommand& cmd = gcodeQueue[currentIndex];
                if (cmd.type == 'G') {
                    execState = ExecState::STARTING_MOVE;
                } else {
                    currentIndex++;
                    robotState->currentCommand = currentIndex;
                    execState = ExecState::SETTING_PEN;
                }
            }
            break;

        case ExecState::STARTING_MOVE: {
            if (currentIndex >= gcodeQueue.size()) {
                execState = ExecState::FINISHED;
                break;
            }

            const GCodeCommand &cmd = gcodeQueue[currentIndex];
            float targetRawX = cmd.hasX ? cmd.x : rawCurrentX;
            float targetRawY = cmd.hasY ? cmd.y : rawCurrentY;

            float targetX = targetRawX * fitScale + fitOffsetX;
            float targetY = targetRawY * fitScale + fitOffsetY;

            if (!kinematics->isWithinWorkspace(targetX, targetY)) {
                Serial.printf("Skipped out-of-workspace point: %.2f, %.2f\n", targetX, targetY);
                currentIndex++;
                robotState->currentCommand = currentIndex;
                execState = ExecState::SETTING_PEN;
                break;
            }

            float deltaX = targetX - robotState->currentX;
            float deltaY = targetY - robotState->currentY;

            if (fabs(deltaX) < MIN_MOVE_MM && fabs(deltaY) < MIN_MOVE_MM)
            {
              rawCurrentX = targetRawX;
              rawCurrentY = targetRawY;

              robotState->currentX = targetX;
              robotState->currentY = targetY;

              currentIndex++;
              robotState->currentCommand = currentIndex;
              execState = ExecState::SETTING_PEN;
              break;
            }

            MotorStepPlan plan;
            if (!kinematics->calculateMotorPlan(robotState->currentX, robotState->currentY,
                                               targetX, targetY, plan)) {
                Serial.printf("Failed IK plan for point: %.2f, %.2f\n", targetX, targetY);
                currentIndex++;
                robotState->currentCommand = currentIndex;
                execState = ExecState::SETTING_PEN;
                break;
            }

            if (!plan.hasMotion()) {
                rawCurrentX = targetRawX;
                rawCurrentY = targetRawY;
                robotState->currentX = targetX;
                robotState->currentY = targetY;
                currentIndex++;
                robotState->currentCommand = currentIndex;
                execState = ExecState::SETTING_PEN;
                break;
            }

            if (!motorController->startMovePlan(plan, (cmd.code == 0))) {
                Serial.println("MotorController rejected motion plan");
                execState = ExecState::FINISHED;
                break;
            }
            robotState->isMoving = true;
            execState = ExecState::WAITING_MOVE;
            break;
        }

        case ExecState::WAITING_MOVE:
            if (!motorController->isBusy()) {
              const GCodeCommand &cmd = gcodeQueue[currentIndex];
              float targetRawX = cmd.hasX ? cmd.x : rawCurrentX;
              float targetRawY = cmd.hasY ? cmd.y : rawCurrentY;

              float targetX = targetRawX * fitScale + fitOffsetX;
              float targetY = targetRawY * fitScale + fitOffsetY;

              rawCurrentX = targetRawX;
              rawCurrentY = targetRawY;

              robotState->currentX = targetX;
              robotState->currentY = targetY;
              robotState->isMoving = false;

              currentIndex++;
              robotState->currentCommand = currentIndex;
              execState = ExecState::SETTING_PEN;
            }
            break;

        case ExecState::FINISHED:
            finishExecution();
            break;

        case ExecState::IDLE:
        default:
            break;
    }
}