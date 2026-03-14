#pragma once

#include <cmath>
#include <stdexcept>
#include <utility>

namespace ship_sim::math {

constexpr double kPi = 3.14159265358979323846;

inline double degToRad(double deg) {
    return deg * kPi / 180.0;
}

inline double radToDeg(double rad) {
    return rad * 180.0 / kPi;
}

inline double normalizeAngle0To2Pi(double rad) {
    const double two_pi = 2.0 * kPi;
    double normalized = std::fmod(rad, two_pi);
    if (normalized < 0.0) {
        normalized += two_pi;
    }
    return normalized;
}

inline std::pair<double, double> localOffsetToLatLonDeg(
    double lat0_deg,
    double lon0_deg,
    double x_m,
    double y_m,
    double earth_radius_m) {
    if (earth_radius_m <= 0.0) {
        throw std::invalid_argument("earth_radius_m must be positive");
    }

    const double lat0_rad = degToRad(lat0_deg);
    const double lon0_rad = degToRad(lon0_deg);
    const double cos_lat0 = std::cos(lat0_rad);
    if (std::abs(cos_lat0) < 1e-6) {
        throw std::invalid_argument("latitude too close to poles for local tangent-plane conversion");
    }

    const double lat_rad = lat0_rad + y_m / earth_radius_m;
    const double lon_rad = lon0_rad + x_m / (earth_radius_m * cos_lat0);
    return {radToDeg(lat_rad), radToDeg(lon_rad)};
}

}  // namespace ship_sim::math
