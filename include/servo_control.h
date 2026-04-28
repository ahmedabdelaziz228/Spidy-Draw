#ifndef SERVO_CONTROL_H
#define SERVO_CONTROL_H

#include <ESP32Servo.h>

class ServoController {
public:
    ServoController();

    void begin();
    void penUp();
    void penDown();
    bool isPenDown() const;
    String getStatusString() const;

private:
    Servo penServo;
    bool penIsDown;
};

#endif