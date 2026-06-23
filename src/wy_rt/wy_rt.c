#include "wy_rt.h"
#include "../../lib/utf8.c/utf8.h"

#define WJCL_LIST_TYPE_IMPLEMENTATION
#include <WJCL/list/wjcl_list_t.h>
#include <WJCL/list/wjcl_linked_list.h>

#define WJCL_STRING_IMPLEMENTATION
#include <WJCL/string/wjcl_string.h>

#include <string.h>

ListT* wy_rt_array_new(size_t item_size) {
    return listT_newA(item_size);
}

void wy_rt_array_add_ptr(ListT* list, void* value_ptr) {
    listT_add(list, value_ptr);
}

void* wy_rt_array_get_ptr(ListT* list, size_t index) {
    return listT_getPtr(list, index);
}

size_t wy_rt_array_get_length(ListT* list) {
    return list->length;
}

void wy_rt_array_free(ListT* list) {
    listT_freeA(list, NULL);
}

char* wy_rt_nth_utf8_char(const char* str, size_t index) {
    if (str == NULL) return NULL;
    utf8_string ustr = make_utf8_string(str);
    if (ustr.str == NULL) return NULL;
    utf8_char uchar = nth_utf8_char(ustr, index);
    if (uchar.str == NULL) return NULL;

    char* result = malloc(uchar.byte_len + 1);
    if (result == NULL) return NULL;
    memcpy(result, uchar.str, uchar.byte_len);
    result[uchar.byte_len] = '\0';
    return result;
}

size_t wy_rt_str_length(const char* str) {
    if (str == NULL) return 0;
    const utf8_string ustr = make_utf8_string(str);
    if (ustr.str == NULL) return 0;
    size_t count = 0;
    utf8_char_iter iter = make_utf8_char_iter(ustr);
    while (next_utf8_char(&iter).byte_len > 0) count++;
    return count;
}

char* wy_rt_str_concat(const char* s1, const char* s2) {
    if (s1 == NULL && s2 == NULL) return NULL;
    if (s1 == NULL) return strdup(s2);
    if (s2 == NULL) return strdup(s1);

    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    char* result = malloc(len1 + len2 + 1);
    if (result == NULL) return NULL;
    memcpy(result, s1, len1);
    memcpy(result + len1, s2, len2);
    result[len1 + len2] = '\0';
    return result;
}
