#include "gcode_parser.h"

bool GCodeParser::extractCoordinate(const String& line, char key, float& value) {
    int pos = line.indexOf(key);
    if (pos == -1) return false;

    String num;
    for (int i = pos + 1; i < line.length(); i++) {
        char c = line.charAt(i);
        if (c == ' ' || c == '\t' || (c >= 'A' && c <= 'Z')) break;
        if (c == '-' || c == '+' || c == '.' || (c >= '0' && c <= '9')) {
            num += c;
        }
    }

    if (num.isEmpty()) return false;

    value = num.toFloat();
    return true;
}

GCodeCommand GCodeParser::parseLine(String line) {
    GCodeCommand cmd;
    cmd.originalLine = line;

    line.trim();
    line.toUpperCase();

    int semicolonPos = line.indexOf(';');
    if (semicolonPos != -1) {
        line = line.substring(0, semicolonPos);
        line.trim();
    }

    while (true) {
        int start = line.indexOf('(');
        int end = line.indexOf(')');
        if (start != -1 && end != -1 && end > start) {
            line.remove(start, end - start + 1);
            line.trim();
        } else {
            break;
        }
    }

    if (line.isEmpty()) {
        return cmd;
    }

    if (line.startsWith("G00") || line.startsWith("G0")) {
        cmd.type = 'G';
        cmd.code = 0;
    } else if (line.startsWith("G01") || line.startsWith("G1")) {
        cmd.type = 'G';
        cmd.code = 1;
    } else if (line.startsWith("M03") || line.startsWith("M3")) {
        cmd.type = 'M';
        cmd.code = 3;
    } else if (line.startsWith("M05") || line.startsWith("M5")) {
        cmd.type = 'M';
        cmd.code = 5;
    } else if (line.startsWith("G21") || line.startsWith("G90")) {
        return cmd;
    } else {
        Serial.println("Unsupported G-code: " + line);
        return cmd;
    }

    float xVal = 0.0f;
    float yVal = 0.0f;

    if (extractCoordinate(line, 'X', xVal)) {
        cmd.x = xVal;
        cmd.hasX = true;
    }

    if (extractCoordinate(line, 'Y', yVal)) {
        cmd.y = yVal;
        cmd.hasY = true;
    }

    cmd.valid = true;
    return cmd;
}