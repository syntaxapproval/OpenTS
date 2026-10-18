#pragma once
/* The MSVC aligned allocator, beside the C library's own malloc.h. */
#include_next <malloc.h>
#include <cstdlib>

inline void * _aligned_malloc(size_t size, size_t align)
{
	void * p = nullptr;
	return(posix_memalign(&p, align < sizeof(void *) ? sizeof(void *) : align, size) == 0 ? p : nullptr);
}
void * _aligned_realloc(void * p, size_t size, size_t align);
inline void _aligned_free(void * p) { free(p); }
