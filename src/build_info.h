#pragma once

#include <fmt/base.h>

#include "caps.h"

extern Cap caps;

namespace utils {

// helper to print build flags
void printBuildFlags() {
    bool capa = false;

#ifdef WS_CAPA
    capa = true;
#endif
    fmt::println("Build flags: WS_CAPA={}", capa);
    fmt::println("Runtime flags: isusermode={}, issetuid={}, hascaps={}", caps.isUserMode(), caps.isSetuid(),
                 caps.hasCaps());
}

} // namespace utils
