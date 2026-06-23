//
// Created by WavJaby on 2026/04/04.
//

#ifndef WENYAN_LLVM_FUNCTION_H
#define WENYAN_LLVM_FUNCTION_H

#include "object.h"
#include "function_info.h"
#include "value_data.h"

void func_defineBuiltins();
Object func_define(const ScientificNotation* funcDefCount, char* funcName);
bool func_defineAddParam(ObjectType type, char* name);
bool func_defineBody();
bool func_defineBodyEnd(Object* funcObj, char* funcName);
bool code_return(const Object* obj);
bool code_returnValue(ValueData* value);

FuncCallInfo* func_callInit(Object* funcObj);
bool func_callArgAdd(FuncCallInfo* funcCall, Object* arg, const YYLTYPE* argLocation);
void func_call(FuncCallInfo* funcCall, const Object* funcObj, ValueData* funcReturn, const YYLTYPE* tokenLoc);
bool func_takeAndCall(const ScientificNotation* count, Object* funcObj, ValueData* valueData, const YYLTYPE* tokenLoc);

#endif //WENYAN_LLVM_FUNCTION_H
