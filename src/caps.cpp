/*
 *  hpc-workspace-v2
 *
 *  caps.cpp
 * 
 *  - privilege elevation with capabilites or setuid implementation
 *
 *  c++ version of workspace utility
 *  a workspace is a temporary directory created in behalf of a user with a limited lifetime.
 *
 *  (c) Holger Berger 2021,2023,2024
 *  (c) Christoph Niethammer 2024
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


#include <cstdlib>


#include "fmt/base.h"

#include "user.h"
#include "caps.h"

extern bool traceflag;
extern bool debugflag;

// TODO: is exit(1) ok here? better error handling?
// TODO: add tracing

// constructor, check if we are setuid or have capabilites (only if linked with libcap)
Cap::Cap() {
    // use those to enable tracing here, is called before aruments are parsed
        // debugflag = true;
        // traceflag = true;
    if (traceflag) fmt::println(stderr, "Trace  : Cap::Cap()");
    if (debugflag) fmt::println(stderr, "Debug  :euid={}, uid={}", geteuid(), getuid());

    issetuid = user::isSetuid();
    hascaps = false;
    isusermode = false;

#ifdef WS_CAPA    
    cap_t caps, oldcaps;
    cap_value_t cap_list[1];

    caps = cap_get_proc();
    oldcaps = cap_dup(caps);

    cap_list[0] = CAP_DAC_OVERRIDE;  // this has to be set for all executables using this!

    if (cap_set_flag(caps, CAP_EFFECTIVE, 1, cap_list, CAP_SET) == -1) {
        fmt::print(stderr, "Error  : problem with capabilities, should not happen\n");
        cap_free(caps);
        cap_free(oldcaps);
        exit(1);
    }

    if (cap_set_proc(caps) == 0) {
        hascaps = true;
        cap_set_proc(oldcaps);
    }

    cap_free(caps);
    cap_free(oldcaps);
#endif

    if (!issetuid && !hascaps) isusermode = true;

    if (debugflag) {
#ifdef WS_CAPA
        fmt::println(stderr, "Debug  : libcap is linked");
#endif
        fmt::println(stderr, "Debug  : issetuid={} hascaps={} isusermode={}",issetuid, hascaps, isusermode);
    }
}


// drop effective capabilities, except CAP_DAC_OVERRIDE | CAP_CHOWN
void Cap::drop_caps(std::vector<cap_value_t> cap_arg, int uid, utils::SrcPos srcpos)
{
    if (traceflag) fmt::println(stderr, "Trace  : Cap::dropcap( {}, {})", uid, srcpos.getSrcPos());
#ifdef WS_CAPA
    if(hascaps) {
        cap_t caps;
        cap_value_t cap_list[cap_arg.size()];

        int arg=0;
        for(const auto &ca: cap_arg) {
            cap_list[arg++] = ca;
        }

        caps = cap_init();

        if (cap_set_flag(caps, CAP_PERMITTED, 1, cap_list, CAP_SET) == -1) {
            fmt::print(stderr, "Error  : problem with capabilities.\n");
            exit(1);
        }

        if (cap_set_proc(caps) != 0) {
            fmt::print(stderr, "Error  : problem dropping capabilities.\n");
            cap_t cap = cap_get_proc();
            char * cap_text = cap_to_text(cap, NULL);
            fmt::print(stderr, "Info   : running with capabilities: {}\n", cap_text);
            cap_free(cap_text);
            cap_free(cap);
            cap_free(caps);
            exit(1);
        }

        cap_free(caps);
    }
#endif

    if (issetuid) {
        if (seteuid(uid)) {
            fmt::print(stderr, "Error  : can not change uid {}.\n", srcpos.getSrcPos());
            exit(1);
        }
    }

}

// remove a capability from the effective set
void Cap::lower_cap(cap_value_t cap, int dbuid, utils::SrcPos srcpos)
{
#ifdef WS_CAPA
    if(hascaps) {   
        cap_t caps; 
        cap_value_t cap_list[1];
        
        caps = cap_get_proc();
        
        cap_list[0] = cap;
        if (cap_set_flag(caps, CAP_EFFECTIVE, 1, cap_list, CAP_CLEAR) == -1) {
            fmt::print(stderr, "Error  : problem with capabilities.\n");
            exit(1);
        }
        
        if (cap_set_proc(caps) == -1) {
            fmt::print(stderr, "Error  : problem lowering capabilities.\n");
            cap_t cap = cap_get_proc();
            fmt::print(stderr, "Info   : running with capabilities: {}\n", cap_to_text(cap, NULL));
            cap_free(cap);
            exit(1);
        }
        
        cap_free(caps);
    }
#endif
    
    if (issetuid) {
        if (seteuid(dbuid)) {
            fmt::print(stderr, "Error  : can not change uid {}.\n", srcpos.getSrcPos());
            exit(1);
        }
    }

}


// add a capability to the effective set
void Cap::raise_cap(int cap, utils::SrcPos srcpos)
{
#ifdef WS_CAPA
    if(hascaps) {
        cap_t caps;
        cap_value_t cap_list[1];

        caps = cap_get_proc();

        cap_list[0] = cap;
        if (cap_set_flag(caps, CAP_EFFECTIVE, 1, cap_list, CAP_SET) == -1) {
            fmt::print(stderr, "Error  : problem with capabilities.\n");
            exit(1);
        }

        if (cap_set_proc(caps) == -1) {
            fmt::print(stderr, "Error  : problem raising capabilities.\n");
            cap_t cap = cap_get_proc();
            fmt::print(stderr, "Running with capabilities: {}\n", cap_to_text(cap, NULL));
            cap_free(cap);
            exit(1);
        }

        cap_free(caps);
    }
#endif

    if (issetuid) {
        if (seteuid(0)) {
            fmt::print(stderr, "Error  : can not change uid {}.\n", srcpos.getSrcPos());
            exit(1);
        }
    }

}
