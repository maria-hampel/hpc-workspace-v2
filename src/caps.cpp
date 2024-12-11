
#include <stdlib.h>


#include "fmt/base.h"

#include "user.h"
#include "caps.h"

extern bool traceflag;
extern bool debugflag;

// TODO: is exit(1) ok here? better error handling?
// TODO: add tracing

// constructor, check if we are setuid or have capabilites (only if linked with libcap)
Cap::Cap() {
    if (traceflag) fmt::println(stderr, "Trace  : Cap::Cap()");
    if (debugflag) fmt::println(stderr, "euid={}, uid={}", geteuid(), getuid());
    if (user::isSetuid()) issetuid = true; else issetuid = false;
    hascaps = false;
    isusermode = false;

#ifdef WS_CAPA    
    cap_t caps, oldcaps;
    cap_value_t cap_list[1];

    oldcaps = caps = cap_get_proc();

    cap_list[0] = CAP_DAC_READ_SEARCH;

    if (cap_set_flag(caps, CAP_EFFECTIVE, 1, cap_list, CAP_SET) == -1) {
        fmt::print(stderr, "Error  : problem with capabilities, should not happen\n");
        exit(1);
    }

    if (cap_set_proc(caps) != -1) {
        hascaps = true;
        cap_set_proc(oldcaps);
    }

    cap_free(caps);
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
        cap_value_t cap_list[1];

        int arg=0;
        for(const auto &ca: cap_arg) {
            cap_list[arg++] = ca;
        }
        //cap_list[0] = cap_arg;

        caps = cap_init();

        // cap_list[0] = CAP_DAC_OVERRIDE;
        // cap_list[1] = CAP_CHOWN;

        if (cap_set_flag(caps, CAP_PERMITTED, 1, cap_list, CAP_SET) == -1) {
            fmt::print(stderr, "Error  : problem with capabilities.\n");
            exit(1);
        }

        if (cap_set_proc(caps) == -1) {
            fmt::print(stderr, "Error  : problem dropping capabilities.\n");
            cap_t cap = cap_get_proc();
            fmt::print(stderr, "Info   : running with capabilities: {}\n", cap_to_text(cap, NULL));
            cap_free(cap);
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
void Cap::lower_cap(int cap, int dbuid, utils::SrcPos srcpos)
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
