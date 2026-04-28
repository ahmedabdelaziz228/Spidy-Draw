#ifndef ROBOT_STATE_H
#define ROBOT_STATE_H

#include <Arduino.h>
#include "config.h"

struct RobotState {
    float currentX = START_X_MM;
    float currentY = START_Y_MM;

    bool penDown = false;
    bool isMoving = false;
    bool isExecuting = false;
    bool stopRequested = false;

    size_t currentCommand = 0;
    size_t totalCommands = 0;

    float fitScale = 1.0f;
    float fitOffsetX = 0.0f;
    float fitOffsetY = 0.0f;
};

#endif