#include "expression.h"

#include "lib/code_gen.h"
#include "compiler_util.h"
#include "object.h"
#include "scope.h"

static inline bool object_sameRegister(const Object* a, const Object* b) {
    return a->type == OBJECT_TYPE_REGISTER && b->type == OBJECT_TYPE_REGISTER &&
        a->value.symbol->index == b->value.symbol->index;
}

static inline bool isExpressionOperationLegal(const ExpOp eop, const ObjectType targetType) {
    if (ExpOp_isArithmetic(eop) && !ObjectType_isNumber(targetType)) {
        if (eop == OP_ADD && targetType == OBJECT_TYPE_STR) {
            // String concatenation is legal
        } else {
            yyerrorf("運算符號『%s』不適用於『%s』之屬\n", expOp2str[eop], objectType2str[targetType]);
            return false;
        }
    }
    if (ExpOp_isBooleanOnly(eop) && targetType != OBJECT_TYPE_BOOL) {
        yyerrorf("運算符號『%s』不適用於『%s』之屬\n", expOp2str[eop], objectType2str[targetType]);
        return false;
    }
    if (eop == OP_MOD && !ObjectType_isInteger(targetType)) {
        yyerrorf("運算符號『%s』不適用於『%s』之屬\n", expOp2str[eop], objectType2str[targetType]);
        return false;
    }
    return true;
}

Object code_expression(const ExpOp eop, const bool opLeft, Object* a, Object* b,
                       const YYLTYPE* aLoc, const YYLTYPE* bLoc) {
    ObjectType aValueType = object_getValueType(a), bValueType = object_getValueType(b);

    const ObjectType targetType = object_getPromotedType(aValueType, bValueType);
    if (targetType == OBJECT_TYPE_UNDEFINED) {
        yyerrorf("『%s』與『%s』不可同算\n", objectType2str[aValueType], objectType2str[bValueType]);
        goto FAILED;
    }

    if (!isExpressionOperationLegal(eop, targetType))
        goto FAILED;

    const ObjectType resultType = ExpOp_isOutputLogic(eop) ? OBJECT_TYPE_BOOL : targetType;
    SymbolData resultReg = object_createRegisterSymbol(resultType);

    char aName[MAX_NAME_LENGTH], bName[MAX_NAME_LENGTH];
    Object regA = object_loadRegAndPromote(a, targetType, aName, MAX_NAME_LENGTH);
    if (regA.type == OBJECT_TYPE_UNDEFINED) goto FAILED;
    Object regB = object_loadRegAndPromote(b, targetType, bName, MAX_NAME_LENGTH);
    if (regB.type == OBJECT_TYPE_UNDEFINED) {
        if (a->type == OBJECT_TYPE_SYMBOL || (regA.type == OBJECT_TYPE_REGISTER && a->type != OBJECT_TYPE_REGISTER))
            object_free(&regA);
        goto FAILED;
    }

    const char* left = opLeft ? aName : bName;
    const char* right = opLeft ? bName : aName;
    IRTextBuilder ir = irTextBuilder(&ctx->code);

    if (targetType == OBJECT_TYPE_STR && eop == OP_ADD) {
        irEmit(&ir, "%%reg%s = call ptr @wy_rt_str_concat(ptr %s, ptr %s)",
               resultReg.name, left, right);
    } else {
        const char* opName = ObjectType_isFloat(targetType) ? opIRFloatNames[eop] : opIRIntNames[eop];
        if (!opName) {
            yyerrorf("運算符號『%s』不適用於『%s』之屬\n", expOp2str[eop], objectType2str[targetType]);
            if (a->type == OBJECT_TYPE_SYMBOL || (regA.type == OBJECT_TYPE_REGISTER && a->type != OBJECT_TYPE_REGISTER))
                object_free(&regA);
            if (b->type == OBJECT_TYPE_SYMBOL || (regB.type == OBJECT_TYPE_REGISTER && b->type != OBJECT_TYPE_REGISTER))
                object_free(&regB);
            goto FAILED;
        }
        irEmit(&ir, "%%reg%s = %s %s %s, %s",
               resultReg.name, opName, objectType2llvmType[targetType], left, right);
    }

    const Object* leftObj = opLeft ? a : b;
    const Object* rightObj = opLeft ? b : a;
    compilerLog("exp %s %s %s -> reg<%s>\n",
                object_print(leftObj), opDebugNames[eop], object_print(rightObj), objectType2str[resultType]);

    if (a->type == OBJECT_TYPE_SYMBOL || (regA.type == OBJECT_TYPE_REGISTER && a->type != OBJECT_TYPE_REGISTER))
        object_free(&regA);
    if (b->type == OBJECT_TYPE_SYMBOL || (regB.type == OBJECT_TYPE_REGISTER && b->type != OBJECT_TYPE_REGISTER))
        object_free(&regB);
    if (!object_sameRegister(a, b)) object_free(a);
    object_free(b);
    return (Object){OBJECT_TYPE_REGISTER, .value.symbol = cloneStruct(SymbolData, &resultReg)};

FAILED:
    if (!object_sameRegister(a, b)) object_free(a);
    object_free(b);
    return (Object){.type = OBJECT_TYPE_UNDEFINED, .value = {}};
}

Object code_expressionMod(ExpOp dop, ExpOp eop, bool op_left, Object* a, Object* b,
                          YYLTYPE* dopLoc, YYLTYPE* eopLoc) {
    if (dop != OP_DIV) {
        yyerrorf("欲問所餘，必先用除\n");
        goto FAILED;
    }
    return code_expression(eop, op_left, a, b, dopLoc, eopLoc);

FAILED:
    if (!object_sameRegister(a, b)) object_free(a);
    object_free(b);
    return (Object){.type = OBJECT_TYPE_UNDEFINED, .value = {}};
}

Object code_expressionChain(ExpOp eop, bool op_left, Object* a, Object* b,
                            YYLTYPE* aLoc, YYLTYPE* bLoc) {
    return code_expression(eop, op_left, a, b, aLoc, bLoc);
}

Object code_expressionChainMod(ExpOp dop, ExpOp eop, bool op_left, Object* a, Object* b,
                               YYLTYPE* dopLoc, YYLTYPE* eopLoc) {
    if (dop != OP_DIV) {
        yyerrorf("欲問所餘，必先用除\n");
        object_free(a);
        object_free(b);
        return (Object){.type = OBJECT_TYPE_UNDEFINED, .value = {}};
    }
    return code_expressionChain(eop, op_left, a, b, dopLoc, eopLoc);
}
