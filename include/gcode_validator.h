/**
 * gcode_validator.h - G-Code Generation Validator and Optimizer
 * 
 * Validates and optimizes G-code before execution on motion control robot
 * 
 * Fathi's module for:
 * - Path validation (bounds, duplicates, continuity)
 * - G-code generation with proper scaling
 * - Command optimization
 * - Metadata generation
 */

#ifndef GCODE_VALIDATOR_H
#define GCODE_VALIDATOR_H

#include <Arduino.h>
#include <vector>
#include <cstring>
#include <cmath>

struct GCodePoint {
    float x;
    float y;
    bool valid;

    GCodePoint() : x(0.0f), y(0.0f), valid(false) {}
    GCodePoint(float x, float y) : x(x), y(y), valid(true) {}

    bool isWithinBounds(float maxX = 200.0f, float maxY = 200.0f) const {
        return valid && x >= 0.0f && x <= maxX && y >= 0.0f && y <= maxY;
    }

    float distanceTo(const GCodePoint& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        return sqrtf(dx * dx + dy * dy);
    }
};

struct GCodePathValidationResult {
    bool valid;
    std::vector<GCodePoint> cleanedPoints;
    const char* errorMessage;
    int duplicatesRemoved;
    int outOfBoundsPoints;
    float pathLength;

    GCodePathValidationResult()
        : valid(false), errorMessage(""), duplicatesRemoved(0),
          outOfBoundsPoints(0), pathLength(0.0f) {}
};

struct GCodeGenerationConfig {
    float workspace_x_min;
    float workspace_x_max;
    float workspace_y_min;
    float workspace_y_max;
    float safe_margin;         // Margin from edge
    float min_segment_length;  // Skip moves shorter than this
    float pen_down_speed;      // Speed multiplier for drawing
    float pen_up_speed;        // Speed multiplier for movement
    bool optimize_commands;    // Remove redundant pen up/down

    GCodeGenerationConfig()
        : workspace_x_min(0.0f), workspace_x_max(200.0f),
          workspace_y_min(0.0f), workspace_y_max(200.0f),
          safe_margin(5.0f), min_segment_length(0.1f),
          pen_down_speed(1.0f), pen_up_speed(1.5f),
          optimize_commands(true) {}
};

class GCodeValidator {
public:
    GCodeValidator(const GCodeGenerationConfig& config = GCodeGenerationConfig())
        : config(config) {}

    /**
     * Validate and clean a sequence of points
     *
     * Checks:
     * - Points are within workspace bounds
     * - No invalid/NaN values
     * - Remove consecutive duplicates
     * - Check minimum segment length
     *
     * Returns: Validation result with cleaned points
     */
    GCodePathValidationResult validatePath(
        const std::vector<GCodePoint>& points) {

        GCodePathValidationResult result;
        result.valid = true;

        if (points.empty()) {
            result.errorMessage = "Empty path";
            result.valid = false;
            return result;
        }

        if (points.size() < 2) {
            result.errorMessage = "Path must have at least 2 points";
            result.valid = false;
            return result;
        }

        // Check bounds and remove duplicates
        GCodePoint lastPoint(-1000.0f, -1000.0f);
        float pathLen = 0.0f;

        for (const auto& pt : points) {
            // Check if point is valid
            if (!isfinite(pt.x) || !isfinite(pt.y) || !pt.valid) {
                result.outOfBoundsPoints++;
                continue;
            }

            // Check bounds
            if (!pt.isWithinBounds(config.workspace_x_max, config.workspace_y_max)) {
                result.outOfBoundsPoints++;
                continue;
            }

            // Remove consecutive duplicates
            if (lastPoint.valid && lastPoint.distanceTo(pt) < 0.01f) {
                result.duplicatesRemoved++;
                continue;
            }

            // Add to cleaned points
            result.cleanedPoints.push_back(pt);
            if (lastPoint.valid) {
                pathLen += lastPoint.distanceTo(pt);
            }
            lastPoint = pt;
        }

        result.pathLength = pathLen;

        // Validate result has minimum points
        if (result.cleanedPoints.size() < 2) {
            result.errorMessage = "Cleaned path has fewer than 2 points";
            result.valid = false;
            return result;
        }

        return result;
    }

    /**
     * Calculate scaling to fit drawing within bounds
     *
     * Args:
     *   minX, minY, maxX, maxY: Bounding box of original drawing
     *   safeMargin: Minimum margin from edge
     *
     * Returns: (scale, offsetX, offsetY) to apply to all coordinates
     */
    void calculateFitTransform(float minX, float minY, float maxX, float maxY,
                               float& outScale, float& outOffsetX, float& outOffsetY) {

        float drawingWidth = maxX - minX;
        float drawingHeight = maxY - minY;

        // Handle edge case of zero-size drawing
        if (drawingWidth <= 0.0f) drawingWidth = 1.0f;
        if (drawingHeight <= 0.0f) drawingHeight = 1.0f;

        // Available space after margins
        float availableWidth = config.workspace_x_max - config.workspace_x_min - (2.0f * config.safe_margin);
        float availableHeight = config.workspace_y_max - config.workspace_y_min - (2.0f * config.safe_margin);

        if (availableWidth <= 0.0f) availableWidth = 1.0f;
        if (availableHeight <= 0.0f) availableHeight = 1.0f;

        // Calculate scale to fit
        float scaleX = availableWidth / drawingWidth;
        float scaleY = availableHeight / drawingHeight;
        outScale = (scaleX < scaleY) ? scaleX : scaleY;

        // Clamp scale to reasonable values
        if (outScale > 10.0f) outScale = 10.0f;
        if (outScale < 0.1f) outScale = 0.1f;

        // Calculate offset for centering
        float scaledWidth = drawingWidth * outScale;
        float scaledHeight = drawingHeight * outScale;

        float centerX = (availableWidth - scaledWidth) / 2.0f;
        float centerY = (availableHeight - scaledHeight) / 2.0f;

        outOffsetX = config.workspace_x_min + config.safe_margin + centerX - (minX * outScale);
        outOffsetY = config.workspace_y_min + config.safe_margin + centerY - (minY * outScale);
    }

    /**
     * Apply transform to point
     */
    GCodePoint applyTransform(const GCodePoint& pt, float scale, float offsetX, float offsetY) const {
        return GCodePoint(pt.x * scale + offsetX, pt.y * scale + offsetY);
    }

    /**
     * Remove redundant pen commands
     * (e.g., pen up followed immediately by pen up)
     */
    void optimizeCommands(std::vector<const char*>& commands) {
        if (!config.optimize_commands || commands.size() < 2) {
            return;
        }

        std::vector<const char*> optimized;
        optimized.push_back(commands[0]);

        for (size_t i = 1; i < commands.size(); i++) {
            const char* current = commands[i];
            const char* previous = optimized.back();

            // Skip redundant pen commands
            if ((strcmp(current, "M3") == 0 && strcmp(previous, "M3") == 0) ||
                (strcmp(current, "M5") == 0 && strcmp(previous, "M5") == 0)) {
                continue;
            }

            optimized.push_back(current);
        }

        commands = optimized;
    }

private:
    GCodeGenerationConfig config;
};

/**
 * G-Code Generation Statistics
 */
struct GCodeStats {
    size_t totalCommands;
    size_t moveCommands;
    size_t penUpCommands;
    size_t penDownCommands;
    float totalDistance;
    float estimatedTimeSeconds;
    float boundsMinX, boundsMinY, boundsMaxX, boundsMaxY;
    bool allPointsWithinBounds;
    const char* warnings;

    GCodeStats()
        : totalCommands(0), moveCommands(0), penUpCommands(0), penDownCommands(0),
          totalDistance(0.0f), estimatedTimeSeconds(0.0f),
          boundsMinX(0.0f), boundsMinY(0.0f), boundsMaxX(0.0f), boundsMaxY(0.0f),
          allPointsWithinBounds(true), warnings("") {}
};

#endif  // GCODE_VALIDATOR_H
