#pragma once
/* MSVC CRT spellings, force-included into every translation unit of the Emscripten build. */
#include <strings.h>
#include <string.h>

#ifndef _stricmp
#define _stricmp strcasecmp
#endif
#ifndef stricmp
#define stricmp strcasecmp
#endif
#define _strnicmp strncasecmp
#ifndef strnicmp
#define strnicmp strncasecmp
#endif
#define _strdup strdup
#define _snprintf snprintf
#define _vsnprintf vsnprintf
