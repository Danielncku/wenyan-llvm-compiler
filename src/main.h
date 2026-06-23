#ifndef MAIN_H
#define MAIN_H
#include <stdbool.h>

#include "object.h"
#include "value_data.h"

bool code_stdoutPrint(ValueData* valueData, bool newLine, const YYLTYPE* loc);
bool code_createVariable(ValueData* valueData, char* name, const YYLTYPE* loc);
bool code_assign(Object* dest, Object* src);

Object code_getLength(Object* obj, const YYLTYPE* loc);
bool code_arrayPush(const Object* arr, Object* val, const YYLTYPE* loc);


#endif
