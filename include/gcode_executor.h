#ifndef GCODE_EXECUTOR_H
#define GCODE_EXECUTOR_H

#include <Arduino.h>
#include <vector>
#include <memory>
#include "robot_state.h"
#include "gcode_parser.h"
#include "motor_control.h"
#include "kinematics.h"
#include "servo_control.h"
#include "gcode_validator.h"

class GCodeExecutor {
public:
    GCodeExecutor(MotorController* motors,
                  Kinematics* kin,
                  ServoController* servo,
                  RobotState* state);

    void setQueue(const std::vector<GCodeCommand>& commands);
    void clearQueue();

    bool start();
    void stop();
    void update();

    bool isExecuting() const;
    bool hasQueue() const;

    size_t getCurrentIndex() const;
    size_t getTotalCommands() const;

private:
    enum class ExecState {
        IDLE,
        STARTING,
        SETTING_PEN,
        WAITING_SERVO,
        STARTING_MOVE,
        WAITING_MOVE,
        FINISHED
    };

    MotorController* motorController;
    Kinematics* kinematics;
    ServoController* servoController;
    RobotState* robotState;

    std::vector<GCodeCommand> gcodeQueue;
    std::unique_ptr<GCodeValidator> validator;

    ExecState execState;
    unsigned long waitUntilMs;
    size_t currentIndex;

    float fitScale;
    float fitOffsetX;
    float fitOffsetY;
    float rawCurrentX;
    float rawCurrentY;

    void prepareQueueFit();
    void executeCurrentCommand();
    void finishExecution();
};

#endif