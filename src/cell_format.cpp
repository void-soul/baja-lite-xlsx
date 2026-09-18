#include "cell_format.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace baja_xlsx {

std::string formatDouble(double value) {
    if (!std::isfinite(value)) {
        return std::string();
    }
    char buf[64];

    // Fast path: integral values are the common case in spreadsheets and need no
    // round-trip precision probing.
    if (value == std::floor(value) && std::fabs(value) < 1e15) {
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(value));
        return std::string(buf);
    }

    for (int precision = 15; precision <= 17; ++precision) {
        std::snprintf(buf, sizeof(buf), "%.*g", precision, value);
        if (std::strtod(buf, nullptr) == value) {
            break;
        }
    }
    return std::string(buf);
}

std::string formatDateSerial(double serial) {
    if (!std::isfinite(serial) || serial < 0) {
        return std::string();
    }
    long long days = static_cast<long long>(std::floor(serial));
    double frac = serial - static_cast<double>(days);
    long long secs = static_cast<long long>(std::llround(frac * 86400.0));
    if (secs >= 86400) {
        secs -= 86400;
        days += 1;
    }

    // Days since 1899-12-30 -> days since 1970-01-01 (Excel serial 25569).
    long long z = days - 25569;
    z += 719468; // Howard Hinnant's civil_from_days offset
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = static_cast<unsigned>(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long y = static_cast<long long>(yoe) + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    unsigned d = doy - (153 * mp + 2) / 5 + 1;
    unsigned m = (mp < 10) ? mp + 3 : mp - 9;
    y += (m <= 2) ? 1 : 0;

    const unsigned hh = static_cast<unsigned>(secs / 3600);
    const unsigned mm = static_cast<unsigned>((secs % 3600) / 60);
    const unsigned ss = static_cast<unsigned>(secs % 60);

    char buf[32];
    if (hh || mm || ss) {
        std::snprintf(buf, sizeof(buf), "%04lld-%02u-%02u %02u:%02u:%02u", y, m, d, hh, mm, ss);
    } else {
        std::snprintf(buf, sizeof(buf), "%04lld-%02u-%02u", y, m, d);
    }
    return std::string(buf);
}

std::string formatTimeOfDay(double serial) {
    if (!std::isfinite(serial) || serial < 0) {
        return std::string();
    }
    const double fraction = serial - std::floor(serial);
    long long secs = static_cast<long long>(std::llround(fraction * 86400.0));
    if (secs >= 86400) secs -= 86400;
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02lld:%02lld:%02lld", secs / 3600,
                  (secs % 3600) / 60, secs % 60);
    return std::string(buffer);
}

} // namespace baja_xlsx
