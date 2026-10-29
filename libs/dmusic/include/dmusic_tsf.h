#pragma once

#include "dmusic.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct tsf;

DMAPI DmResult DmDls_createTsf(void const* buf, size_t len, struct tsf** out);

#ifdef __cplusplus
}
#endif
