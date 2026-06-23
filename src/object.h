//
// Created by WavJaby on 2026/3/2.
//

#ifndef WENYAN_LLVM_OBJECT_H
#define WENYAN_LLVM_OBJECT_H

#include "lib/chinese_number.h"
#include "object_type.h"
#include "compiler_util.h"
#include "control/function_info.h"

#define MAX_NAME_LENGTH 32
#define MAX_OBJECT_DATA_LENGTH 128
#define MAX_REGISTER_NAME_LENGTH 8

#define ObjectType_isNumber(type) ((type) >= OBJECT_TYPE_I32 && (type) <= OBJECT_TYPE_F64)
#define ObjectType_isFloat(type) ((type) == OBJECT_TYPE_F32 || (type) == OBJECT_TYPE_F64)
#define ObjectType_isInteger(type) ((type) == OBJECT_TYPE_I32 || (type) == OBJECT_TYPE_I64)

typedef enum ExpOp {
    OP_GT, // >
    OP_LT, // <
    OP_LE, // <=
    OP_GE, // >=
    OP_EQ, // ==
    OP_NE, // !=
    OP_AN, // &&
    OP_OR, // ||
    OP_ADD, // +
    OP_SUB, // -
    OP_MUL, // *
    OP_DIV, // /
    OP_MOD, // %
} ExpOp;

#define ExpOp_isOutputLogic(op) ((op) <= OP_OR)
#define ExpOp_isArithmetic(op) ((op) >= OP_ADD)
#define ExpOp_isBooleanOnly(op) ((op) == OP_AN || (op) == OP_OR)

typedef struct SymbolData {
    ObjectType type;
    ObjectType elementType; // for OBJECT_TYPE_ARRAY: element type
    char* name;
    int32_t index;

    FuncInfo* funcInfo;
    bool funcArg;
} SymbolData;

typedef struct {
    ObjectType type;
    int capturedIndex; // -1 = local/arg, >=0 = upvalue index in closure

    union {
        bool boolean;
        char* str;
        ScientificNotation* number;
        // Borrowed from symbolMap when OBJECT_TYPE_SYMBOL, owned when in other types
        SymbolData* symbol;
    } value;
} Object;

extern const ObjectType numberType2objectType[];
extern const char* objectType2llvmSuffix[];
extern const char* objectType2llvmType[];
extern const char* objectType2strFormat[];
extern const char* objectType2str[];

const char* object_print(const Object* object);
void object_free(Object* obj);
ObjectType object_getValueType(const Object* obj);
SymbolData object_createRegisterSymbol(ObjectType finalType);

Object object_createStrConst(const char* str);
Object object_createStr(char* str);
Object object_createArray();
Object object_createNumber(const ScientificNotation* number);
Object object_createBool(bool value);
Object object_getIndex(const Object* obj, const Object* index, const YYLTYPE* objLoc, const YYLTYPE* indexLoc);

Object object_nameLiteralOrLoadReg(const Object* src, char* buffer, size_t bufferLen);
Object object_loadRegAndPromote(const Object* src, ObjectType targetType, char* buffer, size_t bufferLen);
bool object_packAsPtr(ObjectType valType, const char* valName, char* buffer, size_t bufferLen);
ObjectType object_getPromotedType(ObjectType a, ObjectType b);

SymbolData* symbol_clone(SymbolData* src);
void symbol_freeReg(SymbolData* src);
void symbol_freeSelf(SymbolData* src);

extern const char* opDebugNames[];
extern const char* opIRIntNames[];
extern const char* opIRFloatNames[];
extern const char* expOp2str[];

#endif //WENYAN_LLVM_OBJECT_H
