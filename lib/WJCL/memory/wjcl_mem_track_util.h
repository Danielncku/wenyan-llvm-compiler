#ifdef MEM_TRACK
#define __malloc(size) untrackedMalloc(size)
#define __calloc(count, size) untrackedCalloc(count, size)
#define __realloc(ptr, size) untrackedRealloc(ptr, size)
#define __free(ptr) untrackedFree(ptr)
#else
#define __malloc(size) malloc(size)
#define __calloc(count, size) calloc(count, size)
#define __realloc(ptr, size) realloc(ptr, size)
#define __free(ptr) free(ptr)
#endif