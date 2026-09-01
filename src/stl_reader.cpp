#include "stl_reader.hpp"
#include <map>
#include <cmath>
#include <limits>

struct Edge {
    std::pair<float, float> p1, p2;
    bool operator<(const Edge& other) const {
        if (p1 != other.p1) return p1 < other.p1;
        return p2 < other.p2;
    }
};

std::optional<Polygon2D> STLReader::projectToXY(const std::string& filename) {
    std::vector<Triangle> triangles;
    
    if (!readAsciiSTL(filename, triangles)) {
        return std::nullopt;
    }
    
    return createOutline(triangles);
}

bool STLReader::readAsciiSTL(const std::string& filename, std::vector<Triangle>& triangles) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    std::getline(file, line);
    
    // Check if it's ASCII STL
    if (line.find("solid") != 0) {
        return false; // Binary STL not supported in this simple version
    }
    
    Triangle tri;
    std::string keyword;
    
    while (file >> keyword) {
        if (keyword == "facet") {
            file >> keyword; // "normal"
            file >> tri.normal.x >> tri.normal.y >> tri.normal.z;
        }
        else if (keyword == "outer") {
            file >> keyword; // "loop"
        }
        else if (keyword == "vertex") {
            Vec3f v;
            file >> v.x >> v.y >> v.z;
            
            static int vertex_count = 0;
            if (vertex_count == 0) tri.v1 = v;
            else if (vertex_count == 1) tri.v2 = v;
            else tri.v3 = v;
            
            vertex_count++;
            if (vertex_count == 3) {
                triangles.push_back(tri);
                vertex_count = 0;
            }
        }
        else if (keyword == "endsolid") {
            break;
        }
    }
    
    return !triangles.empty();
}

Polygon2D STLReader::createOutline(const std::vector<Triangle>& triangles) {
    std::map<Edge, int> edge_count;
    const float EPSILON = 1e-6f;
    
    auto round_point = [EPSILON](const Vec3f& p) -> std::pair<float, float> {
        return {
            std::round(p.x / EPSILON) * EPSILON,
            std::round(p.y / EPSILON) * EPSILON
        };
    };
    
    for (const auto& tri : triangles) {
        // Only consider triangles that are roughly horizontal (normal pointing mostly in Z)
        if (std::abs(tri.normal.z) < 0.9f) continue;
        
        auto p1 = round_point(tri.v1);
        auto p2 = round_point(tri.v2);
        auto p3 = round_point(tri.v3);
        
        auto add_edge = [&edge_count](const auto& a, const auto& b) {
            Edge e{a, b};
            if (e.p2 < e.p1) std::swap(e.p1, e.p2);
            edge_count[e]++;
        };
        
        add_edge(p1, p2);
        add_edge(p2, p3);
        add_edge(p3, p1);
    }
    
    // Find boundary edges (edges that appear only once)
    std::vector<std::pair<float, float>> boundary_points;
    std::map<std::pair<float, float>, std::vector<std::pair<float, float>>> adjacency;
    
    for (const auto& [edge, count] : edge_count) {
        if (count == 1) {
            adjacency[edge.p1].push_back(edge.p2);
            adjacency[edge.p2].push_back(edge.p1);
        }
    }
    
    if (!adjacency.empty()) {
        // Reconstruct polygon by following edges
        auto current = adjacency.begin()->first;
        auto prev = std::make_pair(std::numeric_limits<float>::max(), 
                                   std::numeric_limits<float>::max());
        
        do {
            boundary_points.push_back(current);
            const auto& neighbors = adjacency[current];
            
            std::pair<float, float> next;
            if (neighbors[0] != prev) {
                next = neighbors[0];
            } else if (neighbors.size() > 1) {
                next = neighbors[1];
            } else {
                break;
            }
            
            prev = current;
            current = next;
        } while (current != boundary_points[0] && boundary_points.size() <= adjacency.size());
    }
    
    return {computeConvexHull(boundary_points)};
}

std::vector<std::pair<float, float>> STLReader::computeConvexHull(
    const std::vector<std::pair<float, float>>& points) {
    
    if (points.size() <= 3) return points;
    
    // Simple Graham scan for convex hull
    auto pts = points;
    std::sort(pts.begin(), pts.end());
    
    auto cross = [](const auto& o, const auto& a, const auto& b) {
        return (a.first - o.first) * (b.second - o.second) -
               (a.second - o.second) * (b.first - o.first);
    };
    
    std::vector<std::pair<float, float>> hull;
    
    for (int phase = 0; phase < 2; ++phase) {
        size_t start = hull.size();
        for (const auto& p : pts) {
            while (hull.size() >= start + 2 && 
                   cross(hull[hull.size()-2], hull.back(), p) <= 0) {
                hull.pop_back();
            }
            hull.push_back(p);
        }
        hull.pop_back();
        std::reverse(pts.begin(), pts.end());
    }
    
    return hull;
}