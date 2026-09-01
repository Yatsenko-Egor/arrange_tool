#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <optional>

struct Vec3f {
    float x, y, z;
};

struct Triangle {
    Vec3f normal;
    Vec3f v1, v2, v3;
};

struct Polygon2D {
    std::vector<std::pair<float, float>> points;
    float area() const;
};

inline float Polygon2D::area() const {
    if (points.size() < 3) return 0.0f;
    float sum = 0.0f;
    for (size_t i = 0; i < points.size(); ++i) {
        size_t j = (i + 1) % points.size();
        sum += points[i].first * points[j].second - points[j].first * points[i].second;
    }
    return std::abs(sum) * 0.5f;
}

class STLReader {
public:
    static std::optional<Polygon2D> projectToXY(const std::string& filename);
    
private:
    static bool readAsciiSTL(const std::string& filename, std::vector<Triangle>& triangles);
    static Polygon2D createOutline(const std::vector<Triangle>& triangles);
    static std::vector<std::pair<float, float>> computeConvexHull(const std::vector<std::pair<float, float>>& points);
};