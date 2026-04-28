/**
 * path_format.h - Standard Path Data Format for Motion Control
 * 
 * Defines the contract between image processing (Muhammad) and G-code generation (Fathi)
 * 
 * Coordinate System:
 * - Origin: Top-left of workspace (0, 0)
 * - X-axis: Left to right (increases to the right)
 * - Y-axis: Top to bottom (increases downward)
 * - Unit: Millimeters (mm)
 * - Precision: 3 decimal places (0.001 mm)
 * 
 * Drawing Area:
 * - Safe drawing area: 160×160 mm (within 200×200mm workspace)
 * - Safe margins: 20mm on all sides
 * - Bounds: X [0, 200], Y [0, 200]
 */

#ifndef PATH_FORMAT_H
#define PATH_FORMAT_H

#include <Arduino.h>
#include <vector>
#include <cstring>

struct Point {
    float x;
    float y;

    Point() : x(0.0f), y(0.0f) {}
    Point(float x, float y) : x(x), y(y) {}

    // Check if point is valid (has real values)
    bool isValid() const {
        return isfinite(x) && isfinite(y);
    }

    // Check if point is within workspace bounds
    bool isWithinBounds(float maxX = 200.0f, float maxY = 200.0f) const {
        return x >= 0.0f && x <= maxX && y >= 0.0f && y <= maxY;
    }

    // Distance to another point
    float distanceTo(const Point& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        return sqrtf(dx * dx + dy * dy);
    }
};

struct VectorPath {
    std::vector<Point> points;
    bool closed;           // true = polygon (closed), false = line (open)
    float lengthMm;        // Total path length in mm
    int originalPointCount;  // Points before simplification
    bool isSimplified;     // Whether Douglas-Peucker was applied
    float simplifyTolerance;  // Simplification tolerance used (0 if not simplified)

    VectorPath()
        : closed(false), lengthMm(0.0f), originalPointCount(0),
          isSimplified(false), simplifyTolerance(0.0f) {}

    // Calculate path length based on points
    void calculateLength() {
        lengthMm = 0.0f;
        for (size_t i = 1; i < points.size(); i++) {
            lengthMm += points[i - 1].distanceTo(points[i]);
        }
    }

    // Check if path is valid
    bool isValid() const {
        if (points.size() < 2) return false;
        for (const auto& p : points) {
            if (!p.isValid() || !p.isWithinBounds()) {
                return false;
            }
        }
        return true;
    }

    // Check if path is too small to draw
    bool isTrivial(float minLengthMm = 0.5f) const {
        return lengthMm < minLengthMm;
    }

    // Get path bounds
    void getBounds(float& minX, float& minY, float& maxX, float& maxY) const {
        if (points.empty()) {
            minX = minY = maxX = maxY = 0.0f;
            return;
        }
        minX = maxX = points[0].x;
        minY = maxY = points[0].y;
        for (const auto& p : points) {
            if (p.x < minX) minX = p.x;
            if (p.x > maxX) maxX = p.x;
            if (p.y < minY) minY = p.y;
            if (p.y > maxY) maxY = p.y;
        }
    }
};

struct PathExtractionResult {
    std::vector<VectorPath> paths;
    float totalLength Mm;    // Sum of all path lengths
    int totalPoints;         // Sum of all points across paths
    float minX, minY, maxX, maxY;  // Bounding box of all paths
    bool hasClosedPaths;
    bool hasOpenPaths;
    const char* sourceImage; // Filename or identifier
    const char* processingNotes;  // Optional notes about processing

    PathExtractionResult()
        : totalLengthMm(0.0f), totalPoints(0),
          minX(999999.0f), minY(999999.0f), maxX(-999999.0f), maxY(-999999.0f),
          hasClosedPaths(false), hasOpenPaths(false),
          sourceImage(""), processingNotes("") {}

    // Validate all paths
    bool isValid() const {
        if (paths.empty()) return false;
        for (const auto& path : paths) {
            if (!path.isValid()) return false;
        }
        return minX <= maxX && minY <= maxY;
    }

    // Calculate aggregate statistics
    void calculateStats() {
        if (paths.empty()) {
            totalLengthMm = 0.0f;
            totalPoints = 0;
            minX = minY = maxX = maxY = 0.0f;
            hasClosedPaths = hasOpenPaths = false;
            return;
        }

        totalLengthMm = 0.0f;
        totalPoints = 0;
        minX = minY = 999999.0f;
        maxX = maxY = -999999.0f;
        hasClosedPaths = hasOpenPaths = false;

        for (const auto& path : paths) {
            totalLengthMm += path.lengthMm;
            totalPoints += path.points.size();

            if (path.closed) hasClosedPaths = true;
            else hasOpenPaths = true;

            float pMinX, pMinY, pMaxX, pMaxY;
            path.getBounds(pMinX, pMinY, pMaxX, pMaxY);
            minX = min(minX, pMinX);
            minY = min(minY, pMinY);
            maxX = max(maxX, pMaxX);
            maxY = max(maxY, pMaxY);
        }
    }
};

#endif // PATH_FORMAT_H
