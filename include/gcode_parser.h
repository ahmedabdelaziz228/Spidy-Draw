#ifndef GCODE_PARSER_H
#define GCODE_PARSER_H

#include <Arduino.h>

struct GCodeCommand {
    char type = '\0';       // 'G' or 'M'
    int code = -1;          // 0,1,3,5...
    float x = 0.0f;
    float y = 0.0f;
    bool hasX = false;
    bool hasY = false;
    bool valid = false;
    String originalLine;
};

class GCodeParser {
public:
    GCodeParser() = default;

    GCodeCommand parseLine(String line);
    bool extractCoordinate(const String& line, char key, float& value);
};

#endif