#ifndef WY_RT_H
#define WY_RT_H

#include <stdint.h>
#include <stddef.h>

// Opaque struct to hide ListT implementation
typedef struct ListT ListT;

// Array operations
ListT* wy_rt_array_new(size_t item_size);
void wy_rt_array_add_ptr(ListT* list, void* value_ptr);
void* wy_rt_array_get_ptr(ListT* list, size_t index);
size_t wy_rt_array_get_length(ListT* list);
void wy_rt_array_free(ListT* list);

// String operations
char* wy_rt_nth_utf8_char(const char* str, size_t index);
char* wy_rt_str_concat(const char* s1, const char* s2);
size_t wy_rt_str_length(const char* str);

#endif // WY_RT_H
