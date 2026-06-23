//
// Created by WavJaby on 2026/3/2.
//

#include "object.h"

#include <WJCL/map/wjcl_hash_map.h>

#include "lib/code_gen.h"
#include "compiler_util.h"

const ObjectType numberType2objectType[] = {
    [I32] = OBJECT_TYPE_I32,
    [I64] = OBJECT_TYPE_I64,
    [F64] = OBJECT_TYPE_F64,
};
const char* objectType2llvmSuffix[] = {
    [OBJECT_TYPE_I32] = "i32",
    [OBJECT_TYPE_I64] = "i64",
    [OBJECT_TYPE_F32] = "f32",
    [OBJECT_TYPE_F64] = "f64",
};
const char* objectType2llvmType[] = {
    [OBJECT_TYPE_UNDEFINED] = "void",
    [OBJECT_TYPE_I32] = "i32",
    [OBJECT_TYPE_I64] = "i64",
    [OBJECT_TYPE_F32] = "float",
    [OBJECT_TYPE_F64] = "double",
    [OBJECT_TYPE_STR] = "ptr",
    [OBJECT_TYPE_BOOL] = "i1",
    [OBJECT_TYPE_ARRAY] = "ptr",
    [OBJECT_TYPE_FUNC] = "ptr",
};
const char* objectType2strFormat[] = {
    [OBJECT_TYPE_I32] = "%d",
    [OBJECT_TYPE_I64] = "%lld",
    [OBJECT_TYPE_F64] = "%.16g",
};
const char* objectType2str[] = {
    [OBJECT_TYPE_I32] = "全數",
    [OBJECT_TYPE_I64] = "長數",
    [OBJECT_TYPE_F64] = "浮數",
    [OBJECT_TYPE_NUM] = "數",
    [OBJECT_TYPE_STR] = "字串",
    [OBJECT_TYPE_BOOL] = "爻",
    [OBJECT_TYPE_ARRAY] = "列",
    [OBJECT_TYPE_SYMBOL] = "符",
    [OBJECT_TYPE_FUNC] = "術",
    [OBJECT_TYPE_UNDEFINED] = "虛",
};

const char* opDebugNames[] = {
    [OP_GT] = ">",
    [OP_LT] = "<",
    [OP_LE] = "<=",
    [OP_GE] = ">=",
    [OP_EQ] = "==",
    [OP_NE] = "!=",
    [OP_AN] = "&&",
    [OP_OR] = "||",
    [OP_ADD] = "+",
    [OP_SUB] = "-",
    [OP_MUL] = "*",
    [OP_DIV] = "/",
    [OP_MOD] = "%",
};

const char* opIRIntNames[] = {
    [OP_GT] = "icmp sgt",
    [OP_LT] = "icmp slt",
    [OP_LE] = "icmp sle",
    [OP_GE] = "icmp sge",
    [OP_EQ] = "icmp eq",
    [OP_NE] = "icmp ne",
    [OP_AN] = "and",
    [OP_OR] = "or",
    [OP_ADD] = "add",
    [OP_SUB] = "sub",
    [OP_MUL] = "mul",
    [OP_DIV] = "sdiv",
    [OP_MOD] = "srem",
};

const char* opIRFloatNames[] = {
    [OP_GT] = "fcmp ogt",
    [OP_LT] = "fcmp olt",
    [OP_LE] = "fcmp ole",
    [OP_GE] = "fcmp oge",
    [OP_EQ] = "fcmp oeq",
    [OP_NE] = "fcmp une",
    [OP_ADD] = "fadd",
    [OP_SUB] = "fsub",
    [OP_MUL] = "fmul",
    [OP_DIV] = "fdiv"
};

const char* expOp2str[] = {
    [OP_GT] = "大於",
    [OP_LT] = "小於",
    [OP_LE] = "不大於",
    [OP_GE] = "不小於",
    [OP_EQ] = "等於",
    [OP_NE] = "不等於",
    [OP_AN] = "中無陰乎",
    [OP_OR] = "中有陽乎",
    [OP_ADD] = "加",
    [OP_SUB] = "減",
    [OP_MUL] = "乘",
    [OP_DIV] = "除",
    [OP_MOD] = "所餘幾何",
};


const char* object_print(const Object* object) {
    // Rollong buffer to support multiple print at same time
    static char bufs[3][MAX_OBJECT_DATA_LENGTH];
    static int idx = 0;
    char* buffer = bufs[idx = (idx + 1) % 3];
    switch (object->type) {
    case OBJECT_TYPE_I32:
    case OBJECT_TYPE_I64:
    case OBJECT_TYPE_F64: {
        char* numStr = sciToStr(object->value.number);
        snprintf(buffer, MAX_OBJECT_DATA_LENGTH, "%s", numStr);
        free(numStr);
        break;
    }
    case OBJECT_TYPE_BOOL: strncpy(buffer, object->value.boolean ? "true" : "false", MAX_OBJECT_DATA_LENGTH - 1);
        break;
    case OBJECT_TYPE_STR: snprintf(buffer, MAX_OBJECT_DATA_LENGTH, "\"%s\"", object->value.str);
        break;
    case OBJECT_TYPE_SYMBOL: snprintf(buffer, MAX_OBJECT_DATA_LENGTH, "「%s」", object->value.symbol->name);
        break;
    case OBJECT_TYPE_REGISTER: snprintf(buffer, MAX_OBJECT_DATA_LENGTH, "reg<%s>", objectType2str[object->value.symbol->type]);
        break;
    case OBJECT_TYPE_UNDEFINED: strncpy(buffer, "undefined", MAX_OBJECT_DATA_LENGTH);
        break;
    default: strncpy(buffer, "unknown", MAX_OBJECT_DATA_LENGTH);
        break;
    }
    return buffer;
}

void object_free(Object* obj) {
    if (obj == NULL) return;
    switch (obj->type) {
    case OBJECT_TYPE_STR:
        free(obj->value.str);
        obj->value.str = NULL;
        break;
    case OBJECT_TYPE_I32:
    case OBJECT_TYPE_I64:
    case OBJECT_TYPE_F64:
    case OBJECT_TYPE_NUM:
        free(obj->value.number);
        obj->value.number = NULL;
        break;
    case OBJECT_TYPE_REGISTER:
        symbol_freeReg(obj->value.symbol);
        free(obj->value.symbol);
        obj->value.symbol = NULL;
        break;
    case OBJECT_TYPE_SYMBOL:
    case OBJECT_TYPE_FUNC:
        // Borrowed pointer — owned by symbolMap, do not free here
        obj->value.symbol = NULL;
        break;
    default:
        break;
    }
}

ObjectType object_getValueType(const Object* obj) {
    return obj->type == OBJECT_TYPE_SYMBOL || obj->type == OBJECT_TYPE_REGISTER ? obj->value.symbol->type : obj->type;
}

SymbolData object_createRegisterSymbol(const ObjectType finalType) {
    const SymbolData resultReg = (SymbolData){
        .type = finalType, .name = malloc(MAX_REGISTER_NAME_LENGTH), .index = ctx->registerCount
    };
    snprintf(resultReg.name, MAX_REGISTER_NAME_LENGTH, "%d", ctx->registerCount);
    ++ctx->registerCount;

    return resultReg;
}

Object object_nameLiteralOrLoadReg(const Object* src, char* buffer, const size_t bufferLen) {
    const char* llvmTypeName = objectType2llvmType[object_getValueType(src)];
    switch (src->type) {
    case OBJECT_TYPE_REGISTER:
        snprintf(buffer, bufferLen, "%%reg%s", src->value.symbol->name);
        return *src;
    case OBJECT_TYPE_SYMBOL: {
        if (src->capturedIndex >= 0) {
            snprintf(buffer, bufferLen, "%%upval.%d", src->capturedIndex);
            return (Object){OBJECT_TYPE_REGISTER, .value.symbol = symbol_clone(src->value.symbol)};
        }
        if (src->value.symbol->funcArg) {
            snprintf(buffer, bufferLen, "%%var.%d", src->value.symbol->index);
            return (Object){OBJECT_TYPE_REGISTER, .value.symbol = symbol_clone(src->value.symbol)};
        }
        if (src->value.symbol->type == OBJECT_TYPE_FUNC) {
            snprintf(buffer, bufferLen, "@func_%d_%d",
                     src->value.symbol->funcInfo->contextId, src->value.symbol->index);
            return (Object){OBJECT_TYPE_REGISTER, .value.symbol = symbol_clone(src->value.symbol)};
        }

        // Load symbol value to register（一般變數）
        const SymbolData symbol = object_createRegisterSymbol(src->value.symbol->type);
        IRTextBuilder ir = irTextBuilder(&ctx->code);
        irEmit(&ir, "%%reg%s = load %s, ptr %%var.%d",
               symbol.name, llvmTypeName, src->value.symbol->index);
        snprintf(buffer, bufferLen, "%%reg%s", symbol.name);
        return (Object){OBJECT_TYPE_REGISTER, .value.symbol = cloneStruct(SymbolData, &symbol)};
    }
    case OBJECT_TYPE_I32:
    case OBJECT_TYPE_I64:
    case OBJECT_TYPE_F64: {
        char* numStr = sciToStr(src->value.number);
        snprintf(buffer, bufferLen, "%s", numStr);
        free(numStr);
        return *src;
    }
    case OBJECT_TYPE_BOOL:
        snprintf(buffer, bufferLen, "%d", src->value.boolean);
        return *src;
    case OBJECT_TYPE_STR: {
        const size_t constStrLen = strlen(src->value.str);
        byteBufferWriteFormat(&constBuff, "@str.%d = private unnamed_addr constant [%llu x i8] c\"",
                              constStrCount, constStrLen + 1);
        byteBufferWriteStrUtf8(&constBuff, src->value.str);
        byteBufferWriteStr(&constBuff, "\\00\"\n");

        snprintf(buffer, bufferLen, "@str.%d", constStrCount);
        ++constStrCount;
        return *src;
    }
    default:
        yyerrorf("莫識其類『%s』\n", objectType2str[object_getValueType(src)]);
        return (Object){OBJECT_TYPE_UNDEFINED, .value = {}};
    }
}

Object object_loadRegAndPromote(const Object* src, const ObjectType targetType,
                                char* buffer, const size_t bufferLen) {
    // Update auto-detect object type
    if (src->type == OBJECT_TYPE_SYMBOL &&
        (src->value.symbol->type == OBJECT_TYPE_NUM || src->value.symbol->type == OBJECT_TYPE_AUTO)) {
        src->value.symbol->type = targetType;
        // Symbol is now a direct pointer to the original — no originalPtr needed
    }

    const ObjectType srcType = object_getValueType(src);
    Object regSrc = object_nameLiteralOrLoadReg(src, buffer, bufferLen);

    if (srcType != targetType) {
        const SymbolData resultReg = object_createRegisterSymbol(targetType);
        const char* srcTypeLLVM = objectType2llvmType[srcType];
        const char* targetTypeLLVM = objectType2llvmType[targetType];

        const char* op = NULL;
        switch (srcType) {
        case OBJECT_TYPE_I32:
            if (targetType == OBJECT_TYPE_I64) op = "sext";
            else if (targetType == OBJECT_TYPE_F64) op = "sitofp";
            break;
        case OBJECT_TYPE_I64:
            if (targetType == OBJECT_TYPE_I32) op = "trunc";
            else if (targetType == OBJECT_TYPE_F64) op = "sitofp";
            break;
        case OBJECT_TYPE_F64:
            if (targetType == OBJECT_TYPE_I32) op = "fptosi";
            else if (targetType == OBJECT_TYPE_I64) op = "fptosi";
            break;
        default: break;
        }

        if (src->type == OBJECT_TYPE_SYMBOL) object_free(&regSrc);

        if (op) {
            IRTextBuilder ir = irTextBuilder(&ctx->code);
            irEmit(&ir, "%%reg%s = %s %s %s to %s",
                   resultReg.name, op, srcTypeLLVM, buffer, targetTypeLLVM);
        } else
            return (Object){OBJECT_TYPE_UNDEFINED, .value = {}};

        snprintf(buffer, bufferLen, "%%reg%s", resultReg.name);
        return (Object){OBJECT_TYPE_REGISTER, .value.symbol = cloneStruct(SymbolData, &resultReg)};
    }

    return regSrc;
}

bool object_packAsPtr(const ObjectType valType, const char* valName, char* buffer, const size_t bufferLen) {
    // ptr types pass through unchanged
    if (valType == OBJECT_TYPE_STR || valType == OBJECT_TYPE_ARRAY) {
        snprintf(buffer, bufferLen, "%s", valName);
        return false;
    }

    const char* i64Name = valName;
    const char* convertInstruction[] = {
        [OBJECT_TYPE_I32] = "sext i32",
        [OBJECT_TYPE_BOOL] = "zext i1",
        [OBJECT_TYPE_F64] = "bitcast double",
    };
    char extBuf[MAX_NAME_LENGTH];
    if (valType != OBJECT_TYPE_I64) {
        // Convert to i64 for uniformity
        const char* op = convertInstruction[valType];
        if (!op) {
            yyerrorf("莫識其類『%s』，無由裝之\n", objectType2str[valType]);
            return true;
        }
        SymbolData ext = object_createRegisterSymbol(OBJECT_TYPE_I64);
        IRTextBuilder ir = irTextBuilder(&ctx->code);
        irEmit(&ir, "%%reg%s = %s %s to i64", ext.name, op, valName);
        snprintf(extBuf, MAX_NAME_LENGTH, "%%reg%s", ext.name);
        i64Name = extBuf;
        symbol_freeReg(&ext);
    }

    // Store the i64 value as a fake pointer
    SymbolData r = object_createRegisterSymbol(OBJECT_TYPE_STR);
    IRTextBuilder ir = irTextBuilder(&ctx->code);
    irEmit(&ir, "%%reg%s = inttoptr i64 %s to ptr", r.name, i64Name);
    snprintf(buffer, bufferLen, "%%reg%s", r.name);
    symbol_freeReg(&r);
    return false;
}

ObjectType object_getPromotedType(const ObjectType typeA, const ObjectType typeB) {
    // Both unresolved NUM -> default to i32
    if (typeA == OBJECT_TYPE_NUM && typeB == OBJECT_TYPE_NUM) return OBJECT_TYPE_I32;

    if (typeA == typeB) return typeA;

    if (typeA == OBJECT_TYPE_NUM) return typeB;
    if (typeB == OBJECT_TYPE_NUM) return typeA;

    if (typeA == OBJECT_TYPE_F64 || typeB == OBJECT_TYPE_F64) return OBJECT_TYPE_F64;
    if (typeA == OBJECT_TYPE_I64 || typeB == OBJECT_TYPE_I64) return OBJECT_TYPE_I64;

    return OBJECT_TYPE_UNDEFINED;
}


Object object_createStrConst(const char* str) {
    char* strCopy = strcpy(malloc(strlen(str) + 1), str);
    return (Object){OBJECT_TYPE_STR, .value.str = strCopy};
}

Object object_createStr(char* str) {
    return (Object){OBJECT_TYPE_STR, .value.str = str};
}

Object object_createArray() {
    SymbolData reg = object_createRegisterSymbol(OBJECT_TYPE_ARRAY);
    IRTextBuilder ir = irTextBuilder(&ctx->code);
    irEmit(&ir, "%%reg%s = call ptr @wy_rt_array_new(i64 8)", reg.name);
    return (Object){OBJECT_TYPE_REGISTER, .value.symbol = cloneStruct(SymbolData, &reg)};
}

Object object_createNumber(const ScientificNotation* number) {
    if (number->type == ERROR) {
        yyerrorf("數值莫能辨析\n");
        return (Object){OBJECT_TYPE_UNDEFINED, .value = {}};
    }

    return (Object){numberType2objectType[number->type], .value.number = cloneStruct(ScientificNotation, number)};
}

Object object_createBool(const bool value) {
    return (Object){OBJECT_TYPE_BOOL, .value.boolean = value};
}

Object object_getIndex(const Object* obj, const Object* index, const YYLTYPE* objLoc, const YYLTYPE* indexLoc) {
    const ObjectType objType = object_getValueType(obj);

    if (objType != OBJECT_TYPE_ARRAY && objType != OBJECT_TYPE_STR) {
        yyerrorlf("「%s」非列、非書，無由索之\n", objLoc, objectType2str[objType]);
        return (Object){OBJECT_TYPE_UNDEFINED, .value = {}};
    }
    const ObjectType indexType = object_getValueType(index);
    if (!ObjectType_isNumber(indexType)) {
        yyerrorlf("「%s」非數，無由索其序\n", indexLoc, objectType2str[indexType]);
        return (Object){OBJECT_TYPE_UNDEFINED, .value = {}};
    }

    char objName[MAX_NAME_LENGTH], indexName[MAX_NAME_LENGTH];
    Object regObj = object_nameLiteralOrLoadReg(obj, objName, MAX_NAME_LENGTH);
    // index as i64
    Object regIndex = object_loadRegAndPromote(index, OBJECT_TYPE_I64, indexName, MAX_NAME_LENGTH);

    // 1-based → 0-based
    SymbolData zeroBasedReg = object_createRegisterSymbol(OBJECT_TYPE_I64);
    buffPrintln(&ctx->code, "%%reg%s = sub i64 %s, 1", zeroBasedReg.name, indexName);

    Object result;
    if (objType == OBJECT_TYPE_ARRAY) {
        if (obj->type != OBJECT_TYPE_SYMBOL || obj->value.symbol->elementType == OBJECT_TYPE_UNDEFINED) {
            yyerrorlf("列之元屬未知，無由索之\n", objLoc);
            result = (Object){OBJECT_TYPE_UNDEFINED, .value = {}};
        } else {
            const ObjectType elemType = obj->value.symbol->elementType;
            // ptr to internal list slot
            SymbolData slotReg = object_createRegisterSymbol(OBJECT_TYPE_STR);
            buffPrintln(&ctx->code, "%%reg%s = call ptr @wy_rt_array_get_ptr(ptr %s, i64 %%reg%s)",
                        slotReg.name, objName, zeroBasedReg.name);
            // element value loaded from slot
            SymbolData resultReg = object_createRegisterSymbol(elemType);
            buffPrintln(&ctx->code, "%%reg%s = load %s, ptr %%reg%s",
                        resultReg.name, objectType2llvmType[elemType], slotReg.name);

            symbol_freeReg(&slotReg);
            result = (Object){OBJECT_TYPE_REGISTER, .value.symbol = cloneStruct(SymbolData, &resultReg)};
        }
    } else {
        // ptr to heap-allocated utf8 char
        SymbolData resultReg = object_createRegisterSymbol(OBJECT_TYPE_STR);
        buffPrintln(&ctx->code, "%%reg%s = call ptr @wy_rt_nth_utf8_char(ptr %s, i64 %%reg%s)",
                    resultReg.name, objName, zeroBasedReg.name);
        result = (Object){OBJECT_TYPE_REGISTER, .value.symbol = cloneStruct(SymbolData, &resultReg)};
    }

    symbol_freeReg(&zeroBasedReg);
    if (obj->type == OBJECT_TYPE_SYMBOL) object_free(&regObj);
    if (index->type == OBJECT_TYPE_SYMBOL) object_free(&regIndex);
    if (result.type != OBJECT_TYPE_UNDEFINED)
        compilerLog("index %s[%s] -> %s\n", object_print(obj), object_print(index), object_print(&result));
    return result;
}

SymbolData* symbol_clone(SymbolData* src) {
    SymbolData* new = cloneStruct(SymbolData, src);

    new->name = strdup(src->name);
    return new;
}

void symbol_freeReg(SymbolData* src) {
    free(src->name);
}

void symbol_freeSelf(SymbolData* src) {
    if (src->funcInfo) {
        // Function metadata can contain borrowed/shared entries across scopes.
        // Free list nodes, but avoid deep-freeing values here.
        linkedList_free(&src->funcInfo->params);
        linkedList_free(&src->funcInfo->capturedVars);
        free(src->funcInfo);
    }
    free(src->name);

    free(src);
}
