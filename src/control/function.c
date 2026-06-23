//
// Created by WavJaby on 2026/04/04.
//

#include "function.h"

#include "function_builtin.h"
#include "value_data.h"
#include "lib/code_gen.h"
#include "../compiler_util.h"

Object func_define(const ScientificNotation* funcDefCount, char* funcName) {
    compilerLog("> (function define: %s)\n", funcName);

    const int count = sciToInt32(funcDefCount);
    if (count != 1) {
        yyerrorf("逐一而就，戒之在貪\n");
        return (Object){OBJECT_TYPE_UNDEFINED, .value = {}};
    }

    // Function definition
    SymbolData* symbol = scope_addSymbol(OBJECT_TYPE_FUNC, funcName);
    if (!symbol) {
        yyerrorf("「%s」之名，先已立矣\n", funcName);
        return (Object){OBJECT_TYPE_UNDEFINED, .value = {}};
    }
    // Create function info
    symbol->funcInfo = malloc(sizeof(FuncInfo));
    symbol->funcInfo->returnType = OBJECT_TYPE_UNDEFINED;
    symbol->funcInfo->builtin = NULL;
    linkedList_init(&symbol->funcInfo->params);
    linkedList_init(&symbol->funcInfo->capturedVars);
    symbol->funcInfo->capturedSymbolMap = (Map)map_createFromInfo(capturedMapInfo);

    // Push context; contextId comes from the new context's globally unique id
    context_push();
    symbol->funcInfo->contextId = ctx->id;

    // Push scope stack
    ScopeData* funcScope = scope_pushType(SCOPE_FUNCTION);

    // Init function info
    funcScope->u.funcSymbol = symbol;

    return (Object){OBJECT_TYPE_SYMBOL, .value.symbol = symbol};
}

bool func_defineAddParam(const ObjectType type, char* name) {
    compilerLog("> (function param: %s, type: %s)\n", name, objectType2str[type]);

    // Add parameter to function scope
    SymbolData* symbol = scope_addSymbol(type, name);
    if (!symbol) {
        yyerrorf("「%s」之名，先已立矣\n", name);
        return true;
    }
    symbol->funcArg = true;

    const ScopeData* func_scope = scope_getFunction();
    linkedList_addp(&func_scope->u.funcSymbol->funcInfo->params, false, symbol);

    return false;
}

bool func_defineBody() {
    compilerLog("> (function start body)\n");
    return false;
}

bool func_defineBodyEnd(Object* funcObj, char* funcName) {
    compilerLog("< (function end: %s)\n", funcName);

    ScopeData* func_scope = scope_getFunction();

    // Function definition
    FuncInfo* funcInfo = func_scope->u.funcSymbol->funcInfo;
    const int funcCtxId = funcInfo->contextId;
    const int funcSymId = func_scope->u.funcSymbol->index;
    const bool hasCaptured = funcInfo->capturedVars.length > 0;

    // Emit closure struct type declaration if there are captured vars
    if (hasCaptured) {
        byteBufferWriteFormat(&constBuff, "%%closure.func_%d_%d = type { ptr", funcCtxId, funcSymId);
        linkedList_foreach(&funcInfo->capturedVars, capNode) {
            const CapturedEntry* cap = capNode->value;
            byteBufferWriteFormat(&constBuff, ", %s", objectType2llvmType[cap->symbol->type]);
        }
        byteBufferWriteStr(&constBuff, " }\n");
    }

    byteBufferWriteFormat(&constBuff, "define %s @func_%d_%d(",
                          objectType2llvmType[funcInfo->returnType], funcCtxId, funcSymId);

    // Arguments
    bool firstParam = true;
    linkedList_foreach(&funcInfo->params, node) {
        SymbolData* arg = node->value;
        if (!firstParam) byteBufferWriteFormat(&constBuff, ", ");
        byteBufferWriteFormat(&constBuff, "%s %%var.%d",
                              objectType2llvmType[arg->type], arg->index);
        firstParam = false;

        // Transfer symbol data from symbolMap to paramSymbols
        node->value = symbol_clone(arg);
    }

    // Closure pointer parameter (if any captured vars)
    if (hasCaptured) {
        if (!firstParam) byteBufferWriteFormat(&constBuff, ", ");
        byteBufferWriteFormat(&constBuff, "ptr %%closure");
    }
    byteBufferWriteStr(&constBuff, ") {\n");
    buffPrintln(&constBuff, "%%g_stdout = load ptr, ptr @stdout");

    // Build upvalue load instructions and append to constBuff before function body
    if (hasCaptured) {
        linkedList_foreach(&funcInfo->capturedVars, capNode) {
            const CapturedEntry* cap = capNode->value;
            buffPrintln(&constBuff,
                        "%%upval.ptr.%d = getelementptr inbounds %%closure.func_%d_%d, ptr %%closure, i32 0, i32 %d",
                        cap->capturedIndex, funcCtxId, funcSymId, cap->capturedIndex + 1);
            buffPrintln(&constBuff, "%%upval.%d = load %s, ptr %%upval.ptr.%d",
                        cap->capturedIndex, objectType2llvmType[cap->symbol->type], cap->capturedIndex);
        }
    }

    // Append function body and closing brace to constBuff
    byteBufferWrite(&constBuff, ctx->code.buf, ctx->code.len);
    if (!ctx->hasReturn && funcInfo->returnType == OBJECT_TYPE_UNDEFINED)
        byteBufferWriteFormat(&constBuff, "    ret void\n");
    byteBufferWriteStr(&constBuff, "}\n\n");

    // Cleanup
    scope_dump();
    context_pop();

    // Check function ending
    const Object endFuncObj = scope_findSymbol(funcName);

    if (endFuncObj.type != OBJECT_TYPE_SYMBOL || endFuncObj.value.symbol->type != OBJECT_TYPE_FUNC) {
        yyerrorf("「%s」非術，不可施於術之終\n", funcName);
        return true;
    }

    return false;
}

bool code_return(const Object* obj) {
    compilerLog("return %s\n", object_print(obj));

    ScopeData* func_scope = scope_getFunction();
    if (!func_scope) {
        yyerrorf("乃得當施於術之內，不可在外\n");
        goto FAILED;
    }

    FuncInfo* funcInfo = func_scope->u.funcSymbol->funcInfo;

    // Update function return type
    const ObjectType objType = object_getValueType(obj);
    if (funcInfo->returnType != OBJECT_TYPE_UNDEFINED &&
        object_getPromotedType(funcInfo->returnType, objType) == OBJECT_TYPE_UNDEFINED) {
        yyerrorf("回傳變數類別無法轉換\n");
        goto FAILED;
    }

    // Set function return type to first return value type
    // Resolve NUM to a concrete type (NUM+NUM -> I32 via getPromotedType)
    if (funcInfo->returnType == OBJECT_TYPE_UNDEFINED)
        funcInfo->returnType = object_getPromotedType(objType, objType);

    ctx->hasReturn = true;
    if (objType == OBJECT_TYPE_UNDEFINED) {
        buffPrintln(&ctx->code, "ret void");
    } else {
        char buffer[MAX_NAME_LENGTH];
        Object object = object_loadRegAndPromote(obj, funcInfo->returnType, buffer, MAX_NAME_LENGTH);
        buffPrintln(&ctx->code, "ret %s %s",
                    objectType2llvmType[funcInfo->returnType], buffer);
        object_free(&object);
    }

    return false;
FAILED:
    return true;
}

bool code_returnValue(ValueData* value) {
    const Object* retObj = object_ValueDataListPop(value);
    if (!retObj) compilerLog("return (void)\n");
    if (retObj && code_return(retObj)) return true;
    object_ValueDataListFree(value);
    return false;
}

FuncCallInfo* func_callInit(Object* funcObj) {
    ObjectType objType = object_getValueType(funcObj);

    if (objType != OBJECT_TYPE_FUNC) {
        yyerrorf("呼叫對象「%s」之屬『%s』不為函數\n", funcObj->value.symbol->name, objectType2str[objType]);
        return NULL;
    }

    FuncInfo* funcInfo = funcObj->value.symbol->funcInfo;
    FuncCallInfo* infoPtr = malloc(sizeof(FuncCallInfo));
    infoPtr->func = funcInfo;
    infoPtr->currentArg = funcInfo->params.head->next;
    linkedList_init(&infoPtr->args);
    return infoPtr;
}

bool func_callArgAdd(FuncCallInfo* funcCall, Object* arg, const YYLTYPE* argLocation) {
    const LinkedListNode* currentNode = funcCall->currentArg;
    if (!currentNode) {
        yyerrorlf("函數參數數量不符\n", argLocation);
        return true;
    }

    const SymbolData* paramSymbol = (SymbolData*)currentNode->value;
    const ObjectType objType = object_getValueType(arg);
    const ObjectType paramType = paramSymbol->type;

    if (paramType != objType &&
        object_getPromotedType(paramType, objType) == OBJECT_TYPE_UNDEFINED) {
        yyerrorlf("函數參數類別不符\n", argLocation);
        return true;
    }

    Object* clone = cloneStruct(Object, arg);
    if (arg->type == OBJECT_TYPE_STR && arg->value.str)
        clone->value.str = strdup(arg->value.str);
    else if (ObjectType_isNumber(arg->type) && arg->value.number)
        clone->value.number = cloneStruct(ScientificNotation, arg->value.number);
    else if (arg->type == OBJECT_TYPE_REGISTER && arg->value.symbol)
        clone->value.symbol = symbol_clone(arg->value.symbol);
    linkedList_addp(&funcCall->args, false, clone);
    funcCall->currentArg = funcCall->currentArg->next;

    return false;
}

// Returns best-matching variant index, or -1 if none match.
static int func_resolveBuiltinVariant(const FuncInfoBuiltin* builtin, const LinkedList* args) {
    int fallback = -1;
    for (int vi = 0; vi < builtin->variantCount; vi++) {
        const ObjectType* variant = builtin->variants[vi];
        bool exactMatch = true, compatible = true;
        int j = 0;
        linkedList_foreach(args, node) {
            const Object* arg = node->value;
            const ObjectType argType = object_getValueType(arg);
            if (object_getPromotedType(argType, variant[j]) == OBJECT_TYPE_UNDEFINED) {
                compatible = false;
                break;
            }
            if (argType != variant[j]) exactMatch = false;
            j++;
        }
        if (!compatible) continue;
        if (exactMatch) return vi;
        if (fallback == -1)
            fallback = vi;
    }
    return fallback;
}

void func_call(FuncCallInfo* funcCall, const Object* funcObj, ValueData* funcReturn, const YYLTYPE* tokenLoc) {
    const FuncInfo* funcInfo = funcCall->func;
    const int funcCtxId = funcInfo->contextId;
    const int funcSymId = funcObj->value.symbol->index;

    linkedList_init(&funcReturn->valueList);

    if (funcInfo->returnType != OBJECT_TYPE_UNDEFINED) {
        funcReturn->valueType = funcInfo->returnType;
        ++funcReturn->count;
    }

    ByteBuffer callBuff = byteBufferInit();
    if (funcReturn->count) {
        const SymbolData resultReg = object_createRegisterSymbol(funcInfo->returnType);
        byteBufferWriteFormat(&callBuff, "%%reg%s = ", resultReg.name);
        const Object resultObj = (Object){OBJECT_TYPE_REGISTER, .value.symbol = cloneStruct(SymbolData, &resultReg)};
        linkedList_addp(&funcReturn->valueList, false, cloneStruct(Object, &resultObj));
        compilerLog("call %s(%zu args) -> %s%s\n",
                    funcObj->value.symbol->name,
                    funcCall->args.length,
                    object_print(&resultObj),
                    funcInfo->builtin ? " [builtin]" : "");
    } else {
        compilerLog("call %s(%zu args)%s\n",
                    funcObj->value.symbol->name,
                    funcCall->args.length,
                    funcInfo->builtin ? " [builtin]" : "");
    }

    int builtinVariant = -1;
    const char* llvmTypeName = funcReturn->count ? objectType2llvmType[funcInfo->returnType] : "void";
    if (funcInfo->builtin != NULL) {
        builtinVariant = func_resolveBuiltinVariant(funcInfo->builtin, &funcCall->args);
        if (builtinVariant == -1) {
            yyerrorlf("函數參數類別不符（內建函數無匹配變體）\n", tokenLoc);
            linkedList_freeA(&funcCall->args, (void (*)(void* value))object_free);
            free(funcCall);
            return;
        }

        const FuncInfoBuiltin* builtin = funcInfo->builtin;
        byteBufferWriteFormat(&callBuff, "call %s %s", llvmTypeName, builtin->baseName);
        for (int i = 0; i < builtin->paramCount; i++)
            byteBufferWriteFormat(&callBuff, ".%s", objectType2llvmSuffix[builtin->variants[builtinVariant][i]]);
        byteBufferWriteStr(&callBuff, "(");
    } else {
        byteBufferWriteFormat(&callBuff, "call %s @func_%d_%d(", llvmTypeName, funcCtxId, funcSymId);
    }

    bool first = true;
    int argIdx = 0;
    funcCall->currentArg = funcInfo->params.head->next;
    linkedList_foreach(&funcCall->args, node) {
        const Object* arg = node->value;
        char argBuf[MAX_NAME_LENGTH];
        // for builtins, promote to variant's concrete type (e.g. I32 -> F64)
        const ObjectType argType = object_getValueType(arg);
        const ObjectType paramType = builtinVariant >= 0
                                         ? funcInfo->builtin->variants[builtinVariant][argIdx]
                                         : ((SymbolData*)funcCall->currentArg->value)->type;
        const ObjectType targetType = object_getPromotedType(argType, paramType);

        Object loaded = object_loadRegAndPromote(arg, targetType, argBuf, MAX_NAME_LENGTH);
        if (!first) byteBufferWriteStr(&callBuff, ", ");
        byteBufferWriteFormat(&callBuff, "%s %s", objectType2llvmType[targetType], argBuf);
        if (arg->type == OBJECT_TYPE_SYMBOL) object_free(&loaded);

        first = false;
        argIdx++;
        funcCall->currentArg = funcCall->currentArg->next;
    }

    if (funcInfo->capturedVars.length > 0) {
        SymbolData clsReg = object_createRegisterSymbol(OBJECT_TYPE_UNDEFINED);
        buffPrintln(&ctx->code, "%%reg%s = alloca %%closure.func_%d_%d", clsReg.name, funcCtxId, funcSymId);

        SymbolData fpPtrReg = object_createRegisterSymbol(OBJECT_TYPE_UNDEFINED);
        buffPrintln(&ctx->code, "%%reg%s = getelementptr inbounds %%closure.func_%d_%d, ptr %%reg%s, i32 0, i32 0",
                    fpPtrReg.name, funcCtxId, funcSymId, clsReg.name);
        buffPrintln(&ctx->code, "store ptr @func_%d_%d, ptr %%reg%s", funcCtxId, funcSymId, fpPtrReg.name);

        int capIdx = 0;
        linkedList_foreach(&funcInfo->capturedVars, capNode) {
            const CapturedEntry* cap = capNode->value;
            char capValBuf[MAX_NAME_LENGTH];

            // scope_findSymbol resolves upvalue index from caller's perspective
            Object capObj = scope_findSymbol(cap->symbol->name);
            Object capLoaded = object_nameLiteralOrLoadReg(&capObj, capValBuf, MAX_NAME_LENGTH);
            if (capLoaded.type == OBJECT_TYPE_REGISTER) object_free(&capLoaded);

            SymbolData fieldPtrReg = object_createRegisterSymbol(OBJECT_TYPE_UNDEFINED);
            buffPrintln(&ctx->code, "%%reg%s = getelementptr inbounds %%closure.func_%d_%d, ptr %%reg%s, i32 0, i32 %d",
                        fieldPtrReg.name, funcCtxId, funcSymId, clsReg.name, capIdx + 1);
            buffPrintln(&ctx->code, "store %s %s, ptr %%reg%s",
                        objectType2llvmType[cap->symbol->type], capValBuf, fieldPtrReg.name);
            ++capIdx;
        }

        if (!first) byteBufferWriteStr(&callBuff, ", ");
        byteBufferWriteFormat(&callBuff, "ptr %%reg%s", clsReg.name);
    }

    byteBufferWriteStr(&callBuff, ")");

    // Write to code buffer
    byteBufferWriteFormat(&ctx->code, SCOPE_SPACE_FMT "%.*s\n", SCOPE_SPACE_VAL, callBuff.len, (char*)callBuff.buf);
    byteBufferFree(&callBuff, false);

    linkedList_freeA(&funcCall->args, (void (*)(void* value))object_free);
    free(funcCall);
}

bool func_takeAndCall(const ScientificNotation* count, Object* funcObj, ValueData* valueData, const YYLTYPE* tokenLoc) {
    FuncCallInfo* funcCall = func_callInit(funcObj);
    if (!funcCall) goto FAILED;

    const int32_t argCount = sciToInt32(count);
    for (int32_t i = 0; i < argCount; i++) {
        Object* arg = object_ValueDataListPop(valueData);
        if (!arg) {
            yyerrorlf("取用參數數量不足\n", tokenLoc);
            goto FAILED;
        }
        if (func_callArgAdd(funcCall, arg, tokenLoc)) {
            goto FAILED;
        }
        --valueData->count;
    }

    func_call(funcCall, funcObj, valueData, tokenLoc);
    return false;
FAILED:
    linkedList_freeA(&funcCall->args, (void (*)(void* value))object_free);
    free(funcCall);
    return true;
}
