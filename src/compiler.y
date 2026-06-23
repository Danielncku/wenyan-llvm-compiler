/* Definition section */
%code requires {
    # define YYLTYPE_IS_DECLARED 1
    # define YYLTYPE_IS_TRIVIAL 1
}

%{
    #include "compiler_util.h"
    #include "main.h"
    #include "expression.h"
    #include "value_data.h"
    #include "scope.h"
    #include "control/for.h"
    #include "control/if.h"
    #include "control/while.h"
    #include "control/function.h"

    typedef struct {
        ValueData* values;
        ValueData valueStorage;
        Object* pushTarget;
        FuncCallInfo* call;
        ObjectType functionArgumentType;
    } ParserSession;

    static ParserSession parserSession = {
        .values = NULL,
        .pushTarget = NULL,
        .call = NULL,
        .functionArgumentType = OBJECT_TYPE_UNDEFINED,
    };

    static Object parserCloneObject(const Object* obj) {
        Object clone = *obj;
        if (obj->type == OBJECT_TYPE_STR && obj->value.str)
            clone.value.str = strdup(obj->value.str);
        else if (ObjectType_isNumber(obj->type) && obj->value.number)
            clone.value.number = cloneStruct(ScientificNotation, obj->value.number);
        else if (obj->type == OBJECT_TYPE_REGISTER && obj->value.symbol)
            clone.value.symbol = symbol_clone(obj->value.symbol);
        return clone;
    }
%}

%define parse.error custom
%locations

/* Variable or self-defined structure */
%union {
    ObjectType var_type;

    bool b_var;
    ScientificNotation n_var;
    char *s_var;

    Object obj_val;
    ValueData val_data;

    FuncCallInfo* func_call;

    bool exp_left;
    ExpOp exp_op;
}
/* Token — quick start 最小集合，實作各規則時依需要自行補充 */
%token COMMENT
%token HERE_ARE HERE_IS_A SAID NAME_IT
%token PRINT CALL TO_CALL
%token RETURN BREAK
%token PAST TOPIC SET ITS IS_THUS
%token IF ELSE_IF ELSE WHILE_TRUE FOR TIMES END
%token TO_PERFORM_FUNC REQUIRE_ARGS FUNC_BEGIN FUNC_END_FOR FUNC_END
%token THOSE TAKE PUSH LENGTH

%token <n_var> NUMBER_LIT
%token <b_var> BOOL_LIT
%token <var_type> VAR_TYPE VAR_TYPE_FUNC
%token <s_var> STR_LIT IDENT
%token <exp_op> EXP_MATH_OP EXP_MATH_MOD_OP EXP_LOGIC_OP EXP_BINARY_LOGIC_OP
%token <exp_left> EXP_PREPOSITION

/* %left 範例 — ValueStmt 實作時視衝突補充 */
%left INDEX

/* Nonterminal with return — 實作子規則時依需要自行補充 */
%type <val_data> CreateValueDataListStmt
%type <val_data> FuncCallStmt
%type <obj_val> ValueStmt LitOrVarStmt ValueLiteralStmt VariableStmt ExpressionChainStmt ExpressionStmt ExpressionNextStmt ValueLiteralOrLastStmt

/* For Return — 用於已提供的 ReturnStmt，詳見 YACC_CHEATSHEET.md §優先序宣告 */
%nonassoc LOWER_THAN_EXPR
%nonassoc RETURN

/* Yacc will start at this nonterminal */
%start Program
%%
/* Grammar section */

/* Scope */
Program
    : GlobalScopeStmt
;

GlobalScopeStmt
    : BodyListStmt
;

/* Scope Body */
BodyListStmt
    : BodyListStmt BodyStmt
    |
;

BodyStmt
    : COMMENT STR_LIT
    | OperationStmt
    | ConditionStmt
    | FunctionStmt
;

/* Function */
/* TODO: 函式定義
 * 登錄函式符號、推入 context/scope、逐一登錄參數、結束後彈出。
 * 函式：func_define, func_defineBody, func_defineBodyEnd, func_defineAddParam
 * 注意：參數型別需透過 $<var_type>0 跨規則傳遞；參數列與參數名稱各自是一層規則
 */
FunctionStmt
    : HERE_ARE NUMBER_LIT VAR_TYPE_FUNC NAME_IT IDENT {
        $<obj_val>$ = func_define(&$2, $5);
      } TO_PERFORM_FUNC FunctionArgsStmt FUNC_BEGIN {
        func_defineBody();
      } BodyListStmt FUNC_END_FOR IDENT FUNC_END {
        func_defineBodyEnd(&$<obj_val>6, $13);
      }
;

FunctionArgsStmt
    : REQUIRE_ARGS FunctionArgListStmt
    |
;

FunctionArgListStmt
    : NUMBER_LIT VAR_TYPE {
        parserSession.functionArgumentType = $2;
      } SAID IDENT {
        func_defineAddParam(parserSession.functionArgumentType, $5);
      }
    | FunctionArgListStmt SAID IDENT
      {
        func_defineAddParam(parserSession.functionArgumentType, $3);
      }
;

/* Condition and Operation */
/* TODO: 控制流（FOR / WHILE / IF-ELSEIF-ELSE）
 * 三種分支，每種都有對應的開始與結束 IR 呼叫。
 * 函式：code_forLoop/End, code_whileLoopStart/End, code_if, code_elseIfLabel, code_elseIf, code_else, code_ifEnd
 * 注意：else-if 與 else 皆為可選；IF 結構由三個子規則組成
 */
ConditionStmt
    : IF ExpressionStmt TOPIC {
        code_if(&$2);
      } BodyListStmt ElseIfStmt ElseStmt END {
        code_ifEnd();
      }
    | WHILE_TRUE {
        code_whileLoopStart();
      } BodyListStmt END {
        code_whileLoopEnd(NULL);
      }
    | FOR ValueStmt TIMES {
        code_forLoop(&$2);
      } BodyListStmt END {
        code_forLoopEnd(NULL);
      }
;

ElseIfStmt
    : ElseIfStmt ELSE_IF {
        code_elseIfLabel();
      } ExpressionStmt TOPIC {
        code_elseIf(&$4);
      } BodyListStmt
    |
;

ElseStmt
    : ELSE {
        code_else();
      } BodyListStmt
    |
;

/* TODO: 各種操作語句
 * 涵蓋變數宣告、命名、賦值、函式呼叫、陣列 push、印出、return。
 * 函式：object_ValueDataList*, code_createVariable, code_assign, code_stdoutPrint,
 *       code_arrayPush, code_return, code_returnValue,
 *       func_callInit, func_callArgAdd, func_call, func_takeAndCall
 * 注意：函式呼叫分前置（施）與後置（以施）兩種；mid-rule action 用 $0 傳遞中間值；
 *       呼叫結果後可接命名、return、print 或省略
 */
OperationStmt
    : CreateValueDataListStmt {
        parserSession.values = &$1;
        object_ValueDataListAddDefaults(&$1, &@1);
      } VariableDefineStmt {
        object_ValueDataListFree(&$1);
        parserSession.values = NULL;
      }
    | CreateValueDataListStmt PRINT {
        YYLTYPE printLoc = @2;
        code_stdoutPrint(&$1, true, &printLoc);
        object_ValueDataListFree(&$1);
      }
    | CreateValueDataListStmt RETURN {
        code_returnValue(&$1);
        parserSession.values = NULL;
      }
    | ExpressionChainStmt {
        object_ValueDataListCreate(object_getValueType(&$1), NULL, &parserSession.valueStorage);
        object_ValueDataListAdd(&parserSession.valueStorage, &$1, &@1);
        parserSession.values = &parserSession.valueStorage;
      } VariableDefineStmt {
        object_ValueDataListFree(&parserSession.valueStorage);
        parserSession.values = NULL;
      }
    | ExpressionChainStmt {
        object_ValueDataListCreate(object_getValueType(&$1), NULL, &parserSession.valueStorage);
        object_ValueDataListAdd(&parserSession.valueStorage, &$1, &@1);
        parserSession.values = &parserSession.valueStorage;
      } TAKE NUMBER_LIT TO_CALL VariableStmt {
        func_takeAndCall(&$4, &$6, &parserSession.valueStorage, &@3);
        object_free(&$6);
      } VariableDefineStmt {
        object_ValueDataListFree(&parserSession.valueStorage);
        parserSession.values = NULL;
      }
    | ExpressionChainStmt PAST VariableStmt TOPIC SET ITS IS_THUS {
        yylloc = @3;
        code_assign(&$3, &$1);
      }
    | ExpressionChainStmt PRINT {
        ValueData valData;
        ScientificNotation one = {.type = I32, .fraction = 1, .fractionLen = 0, .exp = 0};
        object_ValueDataListCreate(object_getValueType(&$1), &one, &valData);
        object_ValueDataListAdd(&valData, &$1, &@1);
        YYLTYPE printLoc = @2;
        code_stdoutPrint(&valData, true, &printLoc);
        object_ValueDataListFree(&valData);
      }
    | ExpressionChainStmt {
        object_free(&$1);
      }
    | ValueStmt PRINT {
        YYLTYPE printLoc = @1;
        printLoc.first_column = @1.last_column + 2;
        printLoc.last_column = printLoc.first_column;
        printLoc.first_column_byte = @1.last_column_byte + 2;
        printLoc.last_column_byte = printLoc.first_column_byte;
        yylloc = printLoc;
        ValueData valData;
        ScientificNotation one = {.type = I32, .fraction = 1, .fractionLen = 1, .exp = 0};
        object_ValueDataListCreate(object_getValueType(&$1), &one, &valData);
        object_ValueDataListAdd(&valData, &$1, &@1);
        code_stdoutPrint(&valData, true, &printLoc);
        object_ValueDataListFree(&valData);
      }
    | ValueStmt {
        object_ValueDataListCreate(object_getValueType(&$1), NULL, &parserSession.valueStorage);
        object_ValueDataListAdd(&parserSession.valueStorage, &$1, &@1);
        parserSession.values = &parserSession.valueStorage;
      } VariableDefineStmt {
        object_ValueDataListFree(&parserSession.valueStorage);
        parserSession.values = NULL;
      }
    | PAST VariableStmt TOPIC SET ValueLiteralOrLastStmt IS_THUS {
        yylloc = @6;
        code_assign(&$2, &$5);
      }
    | PUSH VariableStmt {
        parserSession.pushTarget = &$2;
      } PushValueList {
        object_free(&$2);
        parserSession.pushTarget = NULL;
      }
    | FuncCallStmt PRINT {
        YYLTYPE printLoc = @2;
        code_stdoutPrint(&$1, true, &printLoc);
        object_ValueDataListFree(&$1);
      }
    | FuncCallStmt {
        parserSession.values = &$1;
        object_ValueDataListAddDefaults(&$1, &@1);
      } VariableDefineStmt {
        object_ValueDataListFree(&$1);
        parserSession.values = NULL;
      }
    | RETURN ValueStmt {
        code_return(&$2);
      }
    | BREAK {
        code_break();
      }
;

CreateValueDataListStmt
    : HERE_ARE NUMBER_LIT VAR_TYPE {
        object_ValueDataListCreate($3, &$2, &parserSession.valueStorage);
        parserSession.values = &parserSession.valueStorage;
      } SaidValueList {
        $$ = parserSession.valueStorage;
      }
    | HERE_IS_A VAR_TYPE {
        object_ValueDataListCreate($2, NULL, &parserSession.valueStorage);
        parserSession.values = &parserSession.valueStorage;
      } SaidValueList {
        $$ = parserSession.valueStorage;
      }
    | HERE_IS_A VAR_TYPE ValueStmt {
        object_ValueDataListCreate($2, NULL, &$$);
        parserSession.values = &$$;
        object_ValueDataListAdd(&$$, &$3, &@3);
        object_free(&$3);
      }
;

VariableDefineStmt
    : NAME_IT IDENT {
        code_createVariable(parserSession.values, $2, &@2);
      }
    | VariableDefineStmt SAID IDENT {
        code_createVariable(parserSession.values, $3, &@3);
      }
;

SaidValueList
    : SaidValueList SAID ValueStmt {
        object_ValueDataListAdd(parserSession.values, &$3, &@3);
        object_free(&$3);
      }
    | SaidValueList SAID ValueStmt TAKE NUMBER_LIT TO_CALL VariableStmt {
        object_ValueDataListAdd(parserSession.values, &$3, &@3);
        func_takeAndCall(&$5, &$7, parserSession.values, &@4);
        object_free(&$3);
        object_free(&$7);
      }
    |
;

PushValueList
    : EXP_PREPOSITION ValueStmt {
        code_arrayPush(parserSession.pushTarget, &$2, &@2);
      }
    | PushValueList EXP_PREPOSITION ValueStmt {
        code_arrayPush(parserSession.pushTarget, &$3, &@3);
      }
;

FuncCallStmt
    : CALL VariableStmt {
        parserSession.call = func_callInit(&$2);
      } FuncCallArgList {
        linkedList_init(&$$.valueList);
        $$.valueType = OBJECT_TYPE_AUTO;
        $$.count = 0;
        if (parserSession.call)
            func_call(parserSession.call, &$2, &$$, &@1);
        parserSession.call = NULL;
        object_free(&$2);
      }
;

FuncCallArgList
    : FuncCallArgList EXP_PREPOSITION ValueStmt {
        if (parserSession.call)
            func_callArgAdd(parserSession.call, &$3, &@3);
        object_free(&$3);
      }
    |
;

/* Expressions */
/* TODO: 運算式（四則/邏輯，鏈式）
 * 函式：code_expression/Mod, code_expressionChain/Mod
 * 注意：鏈式第一項用 code_expression，後續用 code_expressionChain；需更新 ctx->last_result
 */
ExpressionChainStmt
    : ExpressionStmt {
        $$ = $1;
        ctx->last_result = parserCloneObject(&$1);
      }
    | ExpressionChainStmt ExpressionNextStmt {
        object_free(&$1);
        $$ = $2;
        ctx->last_result = parserCloneObject(&$2);
      }
;

ExpressionStmt
    : EXP_MATH_OP ValueStmt EXP_PREPOSITION ValueStmt {
        $$ = code_expression($1, $3, &$2, &$4, &@2, &@4);
      }
    | EXP_MATH_OP ValueStmt EXP_PREPOSITION ValueStmt EXP_MATH_MOD_OP {
        $$ = code_expressionMod($1, $5, $3, &$2, &$4, &@1, &@5);
      }
    | ValueStmt EXP_LOGIC_OP ValueStmt {
        $$ = code_expression($2, true, &$1, &$3, &@1, &@3);
      }
    | THOSE ValueStmt ValueStmt EXP_BINARY_LOGIC_OP {
        $$ = code_expression($4, true, &$2, &$3, &@2, &@3);
      }
    | FuncCallStmt {
        Object* obj = object_ValueDataListPop(&$1);
        $$ = obj ? *obj : (Object){.type = OBJECT_TYPE_UNDEFINED};
        free(obj);
        object_ValueDataListFree(&$1);
      }
;

ExpressionNextStmt
    : EXP_MATH_OP ValueLiteralOrLastStmt EXP_PREPOSITION ValueLiteralOrLastStmt {
        $$ = code_expressionChain($1, $3, &$2, &$4, &@2, &@4);
      }
    | EXP_MATH_OP ValueLiteralOrLastStmt EXP_PREPOSITION ValueLiteralOrLastStmt EXP_MATH_MOD_OP {
        $$ = code_expressionChainMod($1, $5, $3, &$2, &$4, &@1, &@5);
      }
;

ValueLiteralOrLastStmt
    : ValueStmt
    | ITS {
        $$ = parserCloneObject(&ctx->last_result);
      }
;

/* Value */
/* TODO: 值、字面值、變數查找
 * 函式：object_createStr/Number/Bool, scope_findSymbol, object_getIndex, code_getLength
 * 注意：ITS 取 ctx->last_result；陣列索引與長度為 ValueStmt 的延伸形式
 */
ValueStmt
    : LitOrVarStmt
    | ValueStmt INDEX ValueStmt {
        yylloc = @3;
        $$ = object_getIndex(&$1, &$3, &@1, &@3);
        object_free(&$1);
        object_free(&$3);
      }
    | ValueStmt LENGTH {
        $$ = code_getLength(&$1, &@2);
      }
    | THOSE ValueStmt {
        $$ = $2;
      }
    | ExpressionStmt
;

LitOrVarStmt
    : ValueLiteralStmt
    | VariableStmt
;

ValueLiteralStmt
    : STR_LIT {
        $$ = object_createStr($1);
      }
    | NUMBER_LIT {
        $$ = object_createNumber(&$1);
      }
    | BOOL_LIT {
        $$ = object_createBool($1);
      }
;

VariableStmt
    : IDENT {
        $$ = scope_findSymbol($1);
        free($1);
      }
;

%%

#include "compiler.h"
