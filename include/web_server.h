#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WiFi.h>
#include <WebServer.h>
#include <vector>
#include "gcode_parser.h"
#include "gcode_executor.h"
#include "robot_state.h"

class MotorController;
class Kinematics;
class ServoController;

class WebServerManager {
public:
    WebServerManager(MotorController* motors,
                     Kinematics* kin,
                     GCodeParser* parser,
                     ServoController* servo,
                     GCodeExecutor* executor,
                     RobotState* state);

    void begin();
    void handleClient();
    bool setupWiFi(const char* ssid, const char* password);

private:
    WebServer server;

    MotorController* motorController;
    Kinematics* kinematics;
    GCodeParser* gcodeParser;
    ServoController* servoController;
    GCodeExecutor* gcodeExecutor;
    RobotState* robotState;

    String uploadBuffer;
    std::vector<GCodeCommand> uploadedCommands;
    size_t uploadBytes = 0;

    void handleRoot();
    void handleMove();
    void handleUpload();
    void handleFileUpload();
    void handleUploadText();
    void handleExecute();
    void handleClear();
    void handleHome();
    void handleServo();
    void handleStop();
    void handleStatus();
    void handleProgress();
    void handleGCodeStatus();

    static const char* getHTMLPage();
};

#endif