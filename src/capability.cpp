
#include <stdlib.h>
#include "capability.h"

#include "fmt/base.h"

// TODO: is exit(1) ok here? better error handling?
// TODO: add tracing

// drop effective capabilities, except CAP_DAC_OVERRIDE | CAP_CHOWN
void drop_cap(cap_value_t cap_arg, int dbuid)
{
#ifndef USERMODE
#ifndef SETUID
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
        fmt::print(stderr, "Running with capabilities: {}\n", cap_to_text(cap, NULL));
        cap_free(cap);
        exit(1);
    }

    cap_free(caps);
#else
    // seteuid(0);
    if(seteuid(dbuid)) {
        fmt::print(stderr, "Error  : can not change uid.\n");
        exit(1);
    }
#endif
#endif
}

void drop_cap(cap_value_t cap_arg1, cap_value_t cap_arg2, int dbuid)
{
#ifndef USERMODE
#ifndef SETUID
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
        fmt::print(stderr, "Running with capabilities: {}\n", cap_to_text(cap, NULL));
        cap_free(cap);
        exit(1);
    }
    
    cap_free(caps);
#else
    // seteuid(0);
    if(seteuid(dbuid)) {
        fmt::print(stderr, "Error  : can not change uid.\n");
        exit(1);
    }
#endif
#endif
}


// remove a capability from the effective set
void lower_cap(int cap, int dbuid)
{
#ifndef USERMODE
#ifndef SETUID
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
        fmt::print(stderr, "Running with capabilities: {}\n", cap_to_text(cap, NULL));
        cap_free(cap);
        exit(1);
    }
    
    cap_free(caps);
#else
    // seteuid(0);
    
    if(seteuid(dbuid)) {
        fmt::print(stderr, "Error  : can not change uid.\n");
        exit(1);
    }
#endif
#endif
}


// add a capability to the effective set
void raise_cap(int cap)
{
#ifndef USERMODE
#ifndef SETUID
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
#else
    if (seteuid(0)) {
        fmt::print(stderr, "Error  : can not change uid.\n");
        exit(1);
    }
#endif
#endif
}
