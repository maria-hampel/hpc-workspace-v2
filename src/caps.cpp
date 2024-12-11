
#include <stdlib.h>


#include "fmt/base.h"

#include "user.h"
#include "caps.h"

// TODO: is exit(1) ok here? better error handling?
// TODO: add tracing

// constructor, check if we are setuid or have capabilites (only if linked with libcap)
Cap::Cap() {
    if (user::isSetuid()) issetuid = true; else issetuid = false;
    hascaps = false;
    isusermode = false;

#ifdef WS_CAP    
    cap_t caps, oldcaps;
    cap_value_t cap_list[1];

    oldcaps = caps = cap_get_proc();

    cap_list[0] = CAP_DAC_READ_SEARCH
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
}


// drop effective capabilities, except CAP_DAC_OVERRIDE | CAP_CHOWN
void Cap::drop_cap(cap_value_t cap_arg, int dbuid)
{
#ifdef WS_CAPA
    if(hascaps) {
        cap_t caps;
        cap_value_t cap_list[1];

        cap_list[0] = cap_arg;

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

    if(issetuid && seteuid(dbuid)) {
        fmt::print(stderr, "Error  : can not change uid.\n");
        exit(1);
    }

}

void Cap::drop_cap(cap_value_t cap_arg1, cap_value_t cap_arg2, int dbuid)
{
#ifdef WS_CAPA
    if(hascaps) {
        cap_t caps; 
        cap_value_t cap_list[2];
        
        cap_list[0] = cap_arg1;
        cap_list[1] = cap_arg2;
        
        caps = cap_init();
        
        // cap_list[0] = CAP_DAC_OVERRIDE;
        // cap_list[1] = CAP_CHOWN;
        
        if (cap_set_flag(caps, CAP_PERMITTED, 2, cap_list, CAP_SET) == -1) {
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
  
    if(issetuid && seteuid(dbuid)) {
        fmt::print(stderr, "Error  : can not change uid.\n");
        exit(1);
    }

}


// remove a capability from the effective set
void Cap::lower_cap(int cap, int dbuid)
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
    
    if(issetuid && seteuid(dbuid)) {
        fmt::print(stderr, "Error  : can not change uid.\n");
        exit(1);
    }

}


// add a capability to the effective set
void Cap::raise_cap(int cap)
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

    if (issetuid && seteuid(0)) {
        fmt::print(stderr, "Error  : can not change uid.\n");
        exit(1);
    }

}
