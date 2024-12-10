#ifndef CAPABILITY_H
#define CAPABILITY_H

// FIXME:: instead of ifdef, we could compile different source files with CMAKE options,
// and implement same interface once with capabilities and once with setuid

// TODO: add __LINE__ and __FILE__ again for ease of debugging?

#include <sys/types.h>
#include <unistd.h>


#ifndef WS_SETUID
#include <sys/capability.h>
#else
typedef int cap_value_t;
const int CAP_CHOWN = 0;
const int CAP_DAC_OVERRIDE = 1;
const int CAP_DAC_READ_SEARCH=2;
const int CAP_FOWNER = 3;
#endif

void drop_cap(cap_value_t cap_arg, int dbuid);
void drop_cap(cap_value_t cap_arg1, cap_value_t cap_arg2, int dbuid);
void lower_cap(int cap, int dbuid);
void raise_cap(int cap);

#endif
