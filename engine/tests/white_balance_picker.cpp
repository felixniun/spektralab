#include "white_balance.hpp"

#include <cmath>
#include <cstdio>
#include <utility>

#include <libraw/libraw.h>

int main() {
    LibRaw raw;
    raw.imgdata.idata.colors = 3;
    for (int i = 0; i < 3; ++i) raw.imgdata.color.cam_xyz[i][i] = 1.0;

    for (const auto [temperature, tint] : {std::pair{6500.0, 0.0}, std::pair{4200.0, 25.0}}) {
        double x = 0, y = 0;
        spkhost::temperature_tint_to_xy(temperature, tint, x, y);
        const std::array<double, 3> camera_rgb{x / y, 1.0, (1.0 - x - y) / y};
        double actual_temperature = 0, actual_tint = 0;
        std::string error;
        if (!spkhost::neutral_temperature_tint(raw, camera_rgb, actual_temperature, actual_tint, error) ||
            std::fabs(actual_temperature - temperature) > 5.0 || std::fabs(actual_tint - tint) > 0.5) {
            std::fprintf(stderr, "white balance picker round trip failed: %s (%.2f K, %.2f)\n",
                         error.c_str(), actual_temperature, actual_tint);
            return 1;
        }
    }

    double temperature = 0, tint = 0;
    std::string error;
    if (spkhost::neutral_temperature_tint(raw, {0.0, 1.0, 1.0}, temperature, tint, error)) {
        std::fprintf(stderr, "white balance picker accepted a zero camera channel\n");
        return 1;
    }
    return 0;
}
