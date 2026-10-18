#pragma once
/* MSVC intrinsics the engine uses, for the Emscripten build. */
#include "windows.h"
#include <cstdlib>
#include <emscripten.h>

/* A cycle counter: wasm has none, so this is a microsecond clock, which callers only diff. */
inline unsigned long long __rdtsc() { return (unsigned long long)(emscripten_get_now() * 1000.0); }
inline void __cpuid(int info[4], int) { info[0] = info[1] = info[2] = info[3] = 0; }
#define _ReturnAddress() __builtin_return_address(0)
#define __debugbreak() __builtin_trap()
#define __assume(x) __builtin_assume(x)
