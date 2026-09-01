#define BOOST_PP_VARIADICS 0
#define BOOST_PREPROCESSOR_CONFIG_HPP

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <map>
#include <optional>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <iomanip>

#include <boost/program_options.hpp>

#define _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH 1
#include <polyclipping/clipper.hpp>
#include <libnest2d/backends/clipper/geometries.hpp>
#include <libnest2d/libnest2d.hpp>

namespace po = boost::program_options;

using Point = libnest2d::PointImpl;
using Box = libnest2d::_Box<Point>;
using Item = libnest2d::Item;
using Coord = libnest2d::TCoord<Point>;
using PolygonImpl = libnest2d::PolygonImpl;

// Структура для хранения подробной информации о модели
struct ModelInfo {
    std::string filename;
    double width;
    double height;
    double area;
    double min_x, max_x, min_y, max_y;
    std::vector<std::pair<double, double>> hull;
};

struct Polygon2D {
    std::vector<std::pair<double, double>> points;
    double min_x = 0, max_x = 0, min_y = 0, max_y = 0;

    double area() const {
        if (points.size() < 3) return 0.0;
        double sum = 0.0;
        for (size_t i = 0; i < points.size(); ++i) {
            size_t j = (i + 1) % points.size();
            sum += points[i].first * points[j].second - points[j].first * points[i].second;
        }
        return std::abs(sum) * 0.5;
    }

    double width() const { return max_x - min_x; }
    double height() const { return max_y - min_y; }
};

std::optional<Polygon2D> loadSTLProjection(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return std::nullopt;

    char header[81] = {0};
    file.read(header, 80);
    std::string header_str(header);
    header_str.erase(0, header_str.find_first_not_of(" \t\r\n"));

    std::vector<std::pair<double, double>> points;

    if (header_str.find("solid") == 0) {
        // ASCII STL
        file.seekg(0);
        std::string line;
        std::getline(file, line);

        std::string keyword;
        while (file >> keyword) {
            if (keyword == "vertex") {
                double x, y, z;
                file >> x >> y >> z;
                points.push_back({x, y});
            }
        }
    } else {
        // Binary STL
        uint32_t triangle_count = 0;
        file.read(reinterpret_cast<char*>(&triangle_count), 4);

        for (uint32_t i = 0; i < triangle_count && !file.eof(); ++i) {
            file.seekg(12, std::ios::cur); // skip normal

            for (int v = 0; v < 3; ++v) {
                float coords[3];
                file.read(reinterpret_cast<char*>(coords), 12);
                points.push_back({coords[0], coords[1]});
            }

            file.seekg(2, std::ios::cur); // skip attribute
        }
    }

    if (points.empty()) return std::nullopt;

    // Удаляем дубликаты точек
    std::sort(points.begin(), points.end());
    points.erase(std::unique(points.begin(), points.end()), points.end());

    if (points.size() < 3) return Polygon2D{points};

    // Graham scan для выпуклой оболочки (2D проекция)
    auto cross = [](const auto& o, const auto& a, const auto& b) {
        return (a.first - o.first) * (b.second - o.second) -
               (a.second - o.second) * (b.first - o.first);
    };

    std::vector<std::pair<double, double>> hull;
    for (int phase = 0; phase < 2; ++phase) {
        size_t start = hull.size();
        for (const auto& p : points) {
            while (hull.size() >= start + 2 &&
                   cross(hull[hull.size()-2], hull.back(), p) <= 0) {
                hull.pop_back();
            }
            hull.push_back(p);
        }
        hull.pop_back();
        std::reverse(points.begin(), points.end());
    }

    Polygon2D result{hull};

    // Вычисляем габариты модели
    if (!hull.empty()) {
        result.min_x = result.max_x = hull[0].first;
        result.min_y = result.max_y = hull[0].second;
        for (const auto& p : hull) {
            result.min_x = std::min(result.min_x, p.first);
            result.max_x = std::max(result.max_x, p.first);
            result.min_y = std::min(result.min_y, p.second);
            result.max_y = std::max(result.max_y, p.second);
        }
    }

    return result;
}

int main(int argc, char* argv[]) {
    try {
        // Настройка вывода с фиксированной точностью
        std::cout << std::fixed << std::setprecision(3);

        po::options_description desc("Options");
        desc.add_options()
            ("help,h", "Show help")
            ("bed,b", po::value<std::string>()->required(), "Bed size WxH (e.g., 200x200)")
            ("spacing,s", po::value<double>()->default_value(1.0), "Spacing in mm")
            ("models", po::value<std::vector<std::string>>()->multitoken()->required(), "STL files");

        po::positional_options_description p;
        p.add("models", -1);

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            std::cout << "\nExample: arrange_tool.exe --bed 200x200 --spacing 5 model1.stl model2.stl\n";
            return 0;
        }

        po::notify(vm);

        std::string bed_str = vm["bed"].as<std::string>();
        auto x_pos = bed_str.find('x');
        if (x_pos == std::string::npos) {
            throw std::runtime_error("Invalid bed format. Use WIDTHxHEIGHT (e.g., 200x200)");
        }

        double bed_w = std::stod(bed_str.substr(0, x_pos));
        double bed_h = std::stod(bed_str.substr(x_pos + 1));
        double spacing = vm["spacing"].as<double>();
        auto model_files = vm["models"].as<std::vector<std::string>>();

        const double SCALE = 1000000.0; // мм -> микроны

        // Компактный заголовок
        std::cout << "# 3D PRINTER BED ARRANGEMENT TOOL (SEQUENTIAL SCANLINE)\n";
        std::cout << "# Bed: " << bed_w << "x" << bed_h << " mm, Spacing: " << spacing << " mm\n";
        std::cout << "# Input files: " << model_files.size() << "\n\n";

        std::vector<Item> items;
        std::vector<ModelInfo> model_infos;

        // Загрузка моделей (без подробного вывода)
        for (size_t i = 0; i < model_files.size(); ++i) {
            auto poly = loadSTLProjection(model_files[i]);
            if (!poly) {
                model_infos.push_back({model_files[i], 0, 0, 0, 0, 0, 0, 0, {}});
                continue;
            }

            if (poly->area() < 1.0) {
                model_infos.push_back({model_files[i], 0, 0, 0, 0, 0, 0, 0, {}});
                continue;
            }

            ModelInfo info;
            info.filename = model_files[i];
            info.width = poly->width();
            info.height = poly->height();
            info.area = poly->area();
            info.min_x = poly->min_x;
            info.max_x = poly->max_x;
            info.min_y = poly->min_y;
            info.max_y = poly->max_y;
            info.hull = poly->points;
            model_infos.push_back(info);

            // Нормализуем координаты: сдвигаем модель к (0,0)
            ClipperLib::Path path;
            for (const auto& p : poly->points) {
                path.push_back(ClipperLib::IntPoint(
                    static_cast<ClipperLib::cInt>((p.first - poly->min_x) * SCALE),
                    static_cast<ClipperLib::cInt>((p.second - poly->min_y) * SCALE)
                    ));
            }

            PolygonImpl shape;
            shape.Contour = path;
            Item item(shape);
            item.binId(libnest2d::BIN_ID_UNSET);
            item.priority(static_cast<int>(i));

            items.push_back(item);
        }

        if (items.empty()) {
            std::cerr << "ERROR: No valid models to arrange\n";
            return 1;
        }

        // Предварительная фильтрация моделей
        std::vector<size_t> valid_indices;
        for (size_t i = 0; i < model_infos.size(); ++i) {
            const auto& info = model_infos[i];
            if (info.width > 0 && info.height > 0 && info.width <= bed_w && info.height <= bed_h) {
                valid_indices.push_back(i);
            }
        }

        if (valid_indices.empty()) {
            std::cerr << "ERROR: No models can fit on the bed\n";
            return 1;
        }

        // Координаты для размещения
        double current_x = 0.0;
        double current_y = 0.0;
        double current_row_height = 0.0;

        // Результаты размещения
        std::vector<std::pair<double, double>> placements(items.size(), {-1.0, -1.0});
        int packed_count = 0;

        // Последовательное размещение
        for (size_t idx : valid_indices) {
            const auto& info = model_infos[idx];
            Item& item = items[idx];

            // Проверяем, помещается ли модель в текущей строке по ширине
            if (current_x + info.width > bed_w) {
                current_x = 0.0;
                current_y += current_row_height + spacing;
                current_row_height = 0.0;
            }

            // Проверяем, помещается ли модель по высоте
            if (current_y + info.height > bed_h) {
                continue;
            }

            // Размещаем модель
            placements[idx] = {current_x, current_y};
            item.translate(Point(
                static_cast<Coord>((current_x + info.width/2.0) * SCALE),
                static_cast<Coord>((current_y + info.height/2.0) * SCALE)
                ));
            item.binId(0);
            packed_count++;

            // Обновляем позицию
            current_x += info.width + spacing;
            current_row_height = std::max(current_row_height, info.height);
        }

        // ===== МАШИНОЧИТАЕМЫЙ ВЫВОД =====
        for (size_t i = 0; i < model_infos.size(); ++i) {
            const auto& info = model_infos[i];

            // Проверяем, была ли модель вообще загружена
            if (info.width == 0 && info.height == 0) {
                std::cout << info.filename << ":FAILED:load error\n";
                continue;
            }

            // Проверяем, прошла ли модель предварительную фильтрацию
            bool in_valid = std::find(valid_indices.begin(), valid_indices.end(), i) != valid_indices.end();

            if (!in_valid) {
                if (info.width > bed_w) {
                    std::cout << info.filename << ":FILTERED:width " << info.width << " > bed " << bed_w << "\n";
                } else if (info.height > bed_h) {
                    std::cout << info.filename << ":FILTERED:height " << info.height << " > bed " << bed_h << "\n";
                } else {
                    std::cout << info.filename << ":FILTERED:unknown\n";
                }
            } else if (placements[i].first >= 0) {
                std::cout << info.filename << ":PLACED:"
                          << placements[i].first << "," << placements[i].second << "\n";
            } else {
                std::cout << info.filename << ":SKIPPED:insufficient space\n";
            }
        }

        // Итоговая строка
        std::cout << "\n# SUMMARY:" << packed_count << "/" << model_infos.size() << " placed\n";

    } catch (const std::exception& e) {
        std::cerr << "\nERROR: " << e.what() << "\n";
        return 1;
    }

    return 0;
}