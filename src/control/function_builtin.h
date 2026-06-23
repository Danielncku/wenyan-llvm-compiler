//
// Created by WavJaby on 2026/6/1.
//

#ifndef WENYAN_LLVM_FUNCTION_BUILTIN_H
#define WENYAN_LLVM_FUNCTION_BUILTIN_H

#include "function_info.h"
#include "object_type.h"
#include "scope.h"

static SymbolData* func_addBuiltinFunc(ScopeData* scopeData, const char* name, FuncInfoBuiltin* builtin,
                                        const ObjectType returnType, int paramCount, const ObjectType* paramTypes) {
    const SymbolData symbol = {.type = OBJECT_TYPE_FUNC, .name = strdup(name), .index = -1};
    SymbolData* symbolData = cloneStruct(SymbolData, &symbol);
    if (!symbolData) exit(EXIT_FAILURE);

    FuncInfo* funcInfo = malloc(sizeof(FuncInfo));
    funcInfo->returnType = returnType;
    funcInfo->builtin = builtin;
    linkedList_init(&funcInfo->params);
    linkedList_init(&funcInfo->capturedVars);
    funcInfo->capturedSymbolMap = (Map)map_createFromInfo(capturedMapInfo);

    // Declare all variants of the builtin function
    for (int v = 0; v < builtin->variantCount; v++) {
        byteBufferWriteFormat(&constBuff, "declare %s %s", objectType2llvmType[returnType], builtin->baseName);
        for (int i = 0; i < builtin->paramCount; i++)
            byteBufferWriteFormat(&constBuff, ".%s", objectType2llvmSuffix[builtin->variants[v][i]]);
        byteBufferWriteFormat(&constBuff, "(");
        for (int i = 0; i < builtin->paramCount; i++) {
            if (i != 0) byteBufferWriteFormat(&constBuff, ", ");
            byteBufferWriteFormat(&constBuff, "%s", objectType2llvmType[builtin->variants[v][i]]);
        }
        byteBufferWriteFormat(&constBuff, ")\n");
    }

    for (int i = 0; i < paramCount; i++) {
        SymbolData* param = malloc(sizeof(SymbolData));
        *param = (SymbolData){.type = paramTypes[i], .name = strdup(""), .index = i};
        linkedList_addp(&funcInfo->params, false, param);
    }

    symbolData->funcInfo = funcInfo;
    map_putpp(&scopeData->symbolMap, strdup(name), symbolData);
    return symbolData;
}


const ObjectType* sqrtVariants[] = {
    (ObjectType[]){OBJECT_TYPE_F32},
    (ObjectType[]){OBJECT_TYPE_F64},
};

const ObjectType* powVariants[] = {
    (ObjectType[]){OBJECT_TYPE_F32, OBJECT_TYPE_F32},
    (ObjectType[]){OBJECT_TYPE_F64, OBJECT_TYPE_F64},
};

inline void func_defineBuiltins() {
    ScopeData* mainScope = ctx->scopeStack.head->prev->value;

    static FuncInfoBuiltin sqrtBuiltin = {
        .baseName = "@llvm.sqrt",
        .paramCount = 1,
        .variantCount = 2,
        .variants = sqrtVariants,
    };
    const ObjectType sqrtParams[] = {OBJECT_TYPE_NUM};
    func_addBuiltinFunc(mainScope, "平方根", &sqrtBuiltin, OBJECT_TYPE_F64,
                         sizeof(sqrtParams) / sizeof(ObjectType), sqrtParams);

    static FuncInfoBuiltin powBuiltin = {
        .baseName = "@llvm.pow",
        .paramCount = 2,
        .variantCount = 2,
        .variants = powVariants,
    };
    const ObjectType powParams[] = {OBJECT_TYPE_NUM, OBJECT_TYPE_NUM};
    func_addBuiltinFunc(mainScope, "冪", &powBuiltin, OBJECT_TYPE_F64,
                         sizeof(powParams) / sizeof(ObjectType), powParams);
}

#endif //WENYAN_LLVM_FUNCTION_BUILTIN_H
