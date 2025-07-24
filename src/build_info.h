#pragma once

/*
 *  hpc-workspace-v2
 *
 *  build_info.h
 *
 *  - helper functions
 *
 *  c++ version of workspace utility
 *  a workspace is a temporary directory created in behalf of a user with a limited lifetime.
 *
 *  (c) Holger Berger 2021,2023,2024,2025
 *  (c) Christoph Niethammer 2025
 *
 *  hpc-workspace-v2 is based on workspace by Holger Berger, Thomas Beisel and Martin Hecht
 *
 *  hpc-workspace-v2 is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  hpc-workspace-v2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with workspace-ng  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <fmt/base.h>

#include "caps.h"

extern Cap caps;

namespace utils {

/// helper to print build flags
void printBuildFlags() {
    bool capa = false;

#ifdef WS_CAPA
    capa = true;
#endif
    fmt::println("Build flags: WS_CAPA={}", capa);
    fmt::println("Runtime flags: isusermode={}, issetuid={}, hascaps={}", caps.isUserMode(), caps.isSetuid(),
                 caps.hasCaps());
}

/**
 * helper to print basic version information
 *
 * @param[in] program_name  use program_name for output
 */
void printVersion(std::string program_name) {
#ifdef IS_GIT_REPOSITORY
    fmt::println("{} build from git commit hash {} on top of release {}", program_name, GIT_COMMIT_HASH, WS_VERSION);
#else
    fmt::println("{} version {}", program_name, WS_VERSION);
#endif
}

} // namespace utils
