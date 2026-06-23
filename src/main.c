#define WJCL_STRING_IMPLEMENTATION
#define WJCL_HASH_MAP_IMPLEMENTATION
#define WJCL_LINKED_LIST_IMPLEMENTATION
#include "main.h"

#include <inttypes.h>
#include <stdarg.h>
#include <utf8.c/utf8.h>
#include <string.h>
#include <unistd.h>
#include <WJCL/string/wjcl_string.h>
#include <WJCL/map/wjcl_hash_map.h>

#include "lib/utf8_console.h"
#include "lib/console_color.h"
#include "lib/code_gen.h"
#include "compiler_util.h"
#include "object.h"
#include "scope.h"
#include "control/function.h"

ByteBuffer constBuff = byteBufferInit();
char *inputFilePath = NULL, *inputFileRelativePath = NULL;
bool compileError;
bool verbose = false, lexerVerbose = false, lexerOnly = false;
bool colorEnabled = false;

int constStrCount = 0;

bool code_stdoutPrintObject(Object* src, bool space, bool newLine, const YYLTYPE* loc) {
    if (loc) yylloc = *loc;
    compilerLog("PRINT: %s\n", object_print(src));

    if (space)
        buffPrintln(&ctx->code, "call i64 @fwrite(ptr @space, i64 1, i64 1, ptr %%g_stdout)");

    const ObjectType srcValueType = object_getValueType(src);
    IRTextBuilder ir = irTextBuilder(&ctx->code);
    // Load cache register
    char regName[MAX_NAME_LENGTH];
    Object regSrc = object_nameLiteralOrLoadReg(src, regName, MAX_NAME_LENGTH);
    if (regSrc.type == OBJECT_TYPE_UNDEFINED)
        goto FAILED;

    // Load variable value
    switch (srcValueType) {
    case OBJECT_TYPE_I64:
    case OBJECT_TYPE_I32:
    case OBJECT_TYPE_F64:
    case OBJECT_TYPE_STR: {
        irEmit(&ir, "call i32 (ptr, ...) @printf(ptr @fmt_%s%s, %s %s)",
               objectType2llvmType[srcValueType], newLine ? "_n" : "",
               objectType2llvmType[srcValueType], regName);
        break;
    }
    case OBJECT_TYPE_BOOL: {
        const char* truePtr = newLine ? "@str_true_n" : "@str_true";
        const char* falsePtr = newLine ? "@str_false_n" : "@str_false";
        const uint64_t trueLen = sizeof("陽") - 1 + newLine;
        const uint64_t falseLen = sizeof("陰") - 1 + newLine;

        // Create a pointer to get true, false string
        const SymbolData ptrReg = object_createRegisterSymbol(OBJECT_TYPE_STR);
        irEmit(&ir, "%%reg%s = select i1 %s, ptr %s, ptr %s",
               ptrReg.name, regName, truePtr, falsePtr);

        const SymbolData lenReg = object_createRegisterSymbol(OBJECT_TYPE_I64);
        irEmit(&ir, "%%reg%s = select i1 %s, i64 %"PRIu64", i64 %"PRIu64,
               lenReg.name, regName, trueLen, falseLen);

        irEmit(&ir, "call i64 @fwrite(ptr %%reg%s, i64 1, i64 %%reg%s, ptr %%g_stdout)",
               ptrReg.name, lenReg.name);
        break;
    }
    default:
        goto FAILED;
    }

    if (src->type == OBJECT_TYPE_SYMBOL) object_free(&regSrc);
    object_free(src);
    return false;
FAILED:
    yyerrorf("未能書此值，蓋未備『%s』之法\n", objectType2str[srcValueType]);
    if (src->type == OBJECT_TYPE_SYMBOL) object_free(&regSrc);
    object_free(src);
    return true;
}

bool code_stdoutPrint(ValueData* valueData, bool newLine, const YYLTYPE* loc) {
    if (loc) yylloc = *loc;
    Object* src;
    bool lastIsString = false;
    bool second = false;
    while ((src = object_ValueDataListPop(valueData))) {
        const bool isString = object_getValueType(src) == OBJECT_TYPE_STR;
        // Don't add space between string
        const bool spaceBetween = second && !(lastIsString && isString);

        if (code_stdoutPrintObject(src, spaceBetween, valueData->valueList.length == 0 && newLine, loc)) {
            return true;
        }
        lastIsString = isString;
        second = true;
    }
    return false;
}

// buffPrintln / compilerLog 用法：見 README.md §工具函式速查
bool code_createVariable(ValueData* valueData, char* name, const YYLTYPE* loc) {
    if (loc) yylloc = *loc;
    Object* src = object_ValueDataListPop(valueData);
    const ObjectType srcValueType = object_getValueType(src);
    SymbolData* symbol = scope_addSymbol(srcValueType, name);
    if (!symbol) goto FAILED;

    compilerLog("var 「%s」 <- %s\n", name, object_print(src));

    {
        char regName[MAX_NAME_LENGTH];
        Object regSrc = object_nameLiteralOrLoadReg(src, regName, MAX_NAME_LENGTH);
        if (regSrc.type == OBJECT_TYPE_UNDEFINED) goto FAILED;

        // alloca：在 stack 上為 %%var.N 分配一個 <type> 大小的空間（等同宣告區域變數）
        // store： 把初始值 regName 存入 %%var.N 指向的位址
        // 讀取時：%%regN = load <type>, ptr %%var.N（由 object_nameLiteralOrLoadReg 的 SYMBOL 分支負責）
        const char* llvmTypeName = objectType2llvmType[srcValueType];
        IRTextBuilder ir = irTextBuilder(&ctx->code);
        irEmit(&ir, "%%var.%d = alloca %s", symbol->index, llvmTypeName);
        irEmit(&ir, "store %s %s, ptr %%var.%d", llvmTypeName, regName, symbol->index);
        if (src->type == OBJECT_TYPE_SYMBOL) object_free(&regSrc);
    }

    free(name);
    object_free(src);
    return false;
FAILED:
    free(name);
    object_free(src);
    return true;
}

bool code_assign(Object* dest, Object* src) {
    compilerLog("set %s <- %s\n", object_print(dest), object_print(src));

    if (dest->type != OBJECT_TYPE_SYMBOL) {
        yyerrorf("欲易其值，當先立其名，不可徒施於『%s』也\n", objectType2str[dest->type]);
        goto FAILED;
    }

    const ObjectType destType = dest->value.symbol->type;
    const ObjectType srcType = object_getValueType(src);
    const ObjectType targetType = object_getPromotedType(destType, srcType);
    if (targetType == OBJECT_TYPE_UNDEFINED || targetType != destType) {
        yyerrorf("欲以『%s』易『%s』，二者之屬不合\n",
                 objectType2str[srcType], objectType2str[destType]);
        goto FAILED;
    }

    char regName[MAX_NAME_LENGTH];
    Object regSrc = object_loadRegAndPromote(src, destType, regName, MAX_NAME_LENGTH);
    if (regSrc.type == OBJECT_TYPE_UNDEFINED) goto FAILED;

    IRTextBuilder ir = irTextBuilder(&ctx->code);
    irEmit(&ir, "store %s %s, ptr %%var.%d",
           objectType2llvmType[destType], regName, dest->value.symbol->index);

    if (src->type == OBJECT_TYPE_SYMBOL ||
        (regSrc.type == OBJECT_TYPE_REGISTER && src->type != OBJECT_TYPE_REGISTER))
        object_free(&regSrc);
    object_free(dest);
    object_free(src);
    return false;

FAILED:
    object_free(dest);
    object_free(src);
    return true;
}


Object code_getLength(Object* obj, const YYLTYPE* loc) {
    const ObjectType objType = object_getValueType(obj);
    if (objType != OBJECT_TYPE_ARRAY && objType != OBJECT_TYPE_STR) {
        yyerrorlf("「%s」非列亦非書，無由問其長\n", loc, objectType2str[objType]);
        object_free(obj);
        return (Object){.type = OBJECT_TYPE_UNDEFINED};
    }

    char objName[MAX_NAME_LENGTH];
    Object regObj = object_nameLiteralOrLoadReg(obj, objName, MAX_NAME_LENGTH);
    if (regObj.type == OBJECT_TYPE_UNDEFINED) {
        object_free(obj);
        return (Object){.type = OBJECT_TYPE_UNDEFINED};
    }

    SymbolData resultReg = object_createRegisterSymbol(OBJECT_TYPE_I64);
    const char* fn = objType == OBJECT_TYPE_STR ? "@wy_rt_str_length" : "@wy_rt_array_get_length";
    IRTextBuilder ir = irTextBuilder(&ctx->code);
    irEmit(&ir, "%%reg%s = call i64 %s(ptr %s)", resultReg.name, fn, objName);

    if (obj->type == OBJECT_TYPE_SYMBOL) object_free(&regObj);
    yylloc = *loc;
    compilerLog("length %s -> reg<長數>\n", object_print(obj));
    object_free(obj);
    return (Object){OBJECT_TYPE_REGISTER, .value.symbol = cloneStruct(SymbolData, &resultReg)};
}

bool code_arrayPush(const Object* arr, Object* val, const YYLTYPE* loc) {
    const ObjectType arrType = object_getValueType(arr);
    if (arr->type != OBJECT_TYPE_SYMBOL || arrType != OBJECT_TYPE_ARRAY) {
        yyerrorlf("「%s」非列，無由充之\n", loc, objectType2str[arrType]);
        goto FAILED;
    }

    char arrName[MAX_NAME_LENGTH];
    Object regArr = object_nameLiteralOrLoadReg(arr, arrName, MAX_NAME_LENGTH);
    if (regArr.type == OBJECT_TYPE_UNDEFINED) goto FAILED;

    const ObjectType valType = object_getValueType(val);

    // Define array type when first element is pushed
    if (arr->value.symbol->elementType == OBJECT_TYPE_UNDEFINED)
        arr->value.symbol->elementType = valType;
    else if (arr->value.symbol->elementType != valType) {
        yyerrorlf("列之元屬『%s』與所充屬『%s』不符\n", loc,
                  objectType2str[arr->value.symbol->elementType], objectType2str[valType]);
        object_free(&regArr);
        goto FAILED;
    }

    char valName[MAX_NAME_LENGTH];
    Object regVal = object_nameLiteralOrLoadReg(val, valName, MAX_NAME_LENGTH);
    if (regVal.type == OBJECT_TYPE_UNDEFINED) {
        object_free(&regArr);
        goto FAILED;
    }

    char ptrName[MAX_NAME_LENGTH];
    if (object_packAsPtr(valType, valName, ptrName, MAX_NAME_LENGTH)) {
        object_free(&regArr);
        goto FAILED;
    }

    yylloc = *loc;
    compilerLog("push 「%s」 <- %s\n", arr->value.symbol->name, object_print(val));
    IRTextBuilder ir = irTextBuilder(&ctx->code);
    irEmit(&ir, "call void @wy_rt_array_add_ptr(ptr %s, ptr %s)", arrName, ptrName);

    object_free(&regArr);
    if (val->type == OBJECT_TYPE_SYMBOL) object_free(&regVal);

    object_free(val);
    return false;
FAILED:
    object_free(val);
    return true;
}

void freeAll() {
    // Symbol/function metadata contains borrowed cross-scope references.
    // Let process teardown reclaim it to avoid double-freeing shared entries.
}

void writeOutputHeader() {
    // Extract file name
    if (inputFileRelativePath)
        code("; ModuleID = '%s'", inputFileRelativePath);
    if (inputFilePath)
        code("source_filename = \"%s\"", inputFilePath);

#ifdef WIN32
    // Enable windows cmd utf8 output
    code("\n"
        "declare dllimport i32 @_setmode(i32, i32)\n"
        "declare dllimport i32 @SetConsoleCP(i32)\n"
        "declare dllimport i32 @SetConsoleOutputCP(i32)\n"
        "define dso_local void @utf8_init() {\n"
        "    %%1 = call i32 @_setmode(i32 0, i32 32768)\n"
        "    %%2 = call i32 @_setmode(i32 1, i32 32768)\n"
        "    %%3 = call i32 @SetConsoleCP(i32 65001)\n"
        "    %%4 = call i32 @SetConsoleOutputCP(i32 65001)\n"
        "    ret void\n"
        "}");

    // Get stdout
    code("@stdout = global ptr null\n"
        "declare ptr @__acrt_iob_func(i32)\n");
#else
    code("\n@stdout = external global ptr\n");
#endif

    code(
        "declare i32 @printf(i8*, ...)\n"
        "declare i32 @_write(i32, ptr, i32)\n"
        "declare i64 @fwrite(ptr, i64, i64, ptr)\n"
        "declare ptr @wy_rt_nth_utf8_char(ptr, i64)\n"
        "declare ptr @wy_rt_str_concat(ptr, ptr)\n"
        "declare ptr @wy_rt_array_new(i64)\n"
        "declare void @wy_rt_array_add_ptr(ptr, ptr)\n"
        "declare ptr @wy_rt_array_get_ptr(ptr, i64)\n"
        "declare i64 @wy_rt_array_get_length(ptr)\n"
        "declare i64 @wy_rt_str_length(ptr)\n"
        "\n"
        "@fmt_i32_n = private unnamed_addr constant [4 x i8] c\"%%d\\0A\\00\"\n"
        "@fmt_i32 = private unnamed_addr constant [3 x i8] c\"%%d\\00\"\n"
        "@fmt_i64_n = private unnamed_addr constant [6 x i8] c\"%%lld\\0A\\00\"\n"
        "@fmt_i64 = private unnamed_addr constant [5 x i8] c\"%%lld\\00\"\n"
        "@fmt_float_n = private unnamed_addr constant [4 x i8] c\"%%f\\0A\\00\"\n"
        "@fmt_float = private unnamed_addr constant [3 x i8] c\"%%f\\00\"\n"
        "@fmt_double_n = private unnamed_addr constant [4 x i8] c\"%%g\\0A\\00\"\n"
        "@fmt_double = private unnamed_addr constant [3 x i8] c\"%%g\\00\"\n"
        "@fmt_ptr_n = private unnamed_addr constant [4 x i8] c\"%%s\\0A\\00\"\n"
        "@fmt_ptr = private unnamed_addr constant [3 x i8] c\"%%s\\00\"\n"
        "@space = private unnamed_addr constant [1 x i8] c\" \"\n"
    );

    // True False display
    const char trueStr[] = "陽", falseStr[] = "陰";
    byteBufferWriteFormat(&constBuff, "@str_true = private unnamed_addr constant [%llu x i8] c\"", sizeof(trueStr));
    byteBufferWriteStrUtf8(&constBuff, trueStr);
    byteBufferWriteStr(&constBuff, "\\00\"\n");
    byteBufferWriteFormat(&constBuff, "@str_true_n = private unnamed_addr constant [%llu x i8] c\"",
                          sizeof(trueStr) + 1);
    byteBufferWriteStrUtf8(&constBuff, trueStr);
    byteBufferWriteStr(&constBuff, "\\0A\\00\"\n");

    byteBufferWriteFormat(&constBuff, "@str_false = private unnamed_addr constant [%llu x i8] c\"", sizeof(falseStr));
    byteBufferWriteStrUtf8(&constBuff, falseStr);
    byteBufferWriteStr(&constBuff, "\\00\"\n");
    byteBufferWriteFormat(&constBuff, "@str_false_n = private unnamed_addr constant [%llu x i8] c\"",
                          sizeof(falseStr) + 1);
    byteBufferWriteStrUtf8(&constBuff, falseStr);
    byteBufferWriteStr(&constBuff, "\\0A\\00\"\n");
}

void writeOutputMain() {
    code("");
    code("define i32 @main() {");
#ifdef WIN32
    // Enable windows cmd utf8 output
    code("    call void @utf8_init()");
    // Get stdout
    code("    %%_stdout = call ptr @__acrt_iob_func(i32 1)");
    code("    store ptr %%_stdout, ptr @stdout");
#endif
    code("    %%g_stdout = load ptr, ptr @stdout");

    byteBufferWriteToFile(&ctx->code, yyout);
    code("    ret i32 0");
    code("}");
}

void printHelp(char* progName) {
    printf("用式：%s [諸項] <源書> [成器]\n", progName);
    printf("諸項：\n");
    printf("  -v  詳述其事\n");
    printf("  -l  詳述解詞\n");
    printf("  -x  唯解其詞\n");
    printf("  -c  顯色\n");
    printf("  -h  示此指南而退\n");
}

int main(int argc, char* argv[]) {
    scope_init();

    char* outputFilePath = NULL;

    // Read command args
    char** argv_p = &argv[1];
    for (int i = 1; i < argc; i++) {
        if ((*argv_p)[0] == '-') {
            switch ((*argv_p)[1]) {
            case 'v': verbose = true;
                break;
            case 'l': lexerVerbose = true;
                break;
            case 'x': lexerOnly = true;
                break;
            case 'c': colorEnabled = true;
                break;
            case 'h': printHelp(argv[0]);
                return 0;
            default:
                fprintf(stderr, "謬矣：未識之項：%s\n", *argv_p);
                printHelp(argv[0]);
                return 1;
            }
        } else {
            // Read input and output file
            if (!inputFilePath)
                inputFilePath = *argv_p;
            else if (!outputFilePath)
                outputFilePath = *argv_p;
            else {
                fprintf(stderr, "謬矣：所引過繁\n");
                printHelp(argv[0]);
                return 1;
            }
        }

        ++argv_p;
    }

    color_init(colorEnabled);

    if (inputFilePath && !((yyin = fopen(inputFilePath, "rb")))) {
        fprintf(stderr, "謬矣：『%s』未見或無從啟之\n", inputFilePath);
        return 1;
    }
    if (outputFilePath && !((yyout = fopen(outputFilePath, "w")))) {
        fprintf(stderr, "謬矣：『%s』未見或無從啟之\n", outputFilePath);
        return 1;
    }

    if (!yyin && !yyout) {
        yyin = stdin;
        yyout = stdout;
        stdinMode = true;
        printf("===== 取言於標準輸入以析之 =====\n");
    } else if (!yyout) {
        yyout = stdout;
    }

    if (inputFilePath) {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd))) {
            for (char *p = cwd; *p; p++) if (*p == '\\') *p = '/';
            for (char *p = inputFilePath; *p; p++) if (*p == '\\') *p = '/';
            size_t cwdLen = strlen(cwd);
            if (strncmp(inputFilePath, cwd, cwdLen) == 0 && inputFilePath[cwdLen] == '/')
                inputFileRelativePath = inputFilePath + cwdLen + 1;
        }
        if (!inputFileRelativePath)
            inputFileRelativePath = inputFilePath;
    }


    yylineno = 1;

    if (lexerOnly) {
        while (yylex()) {
        }
        fclose(yyin);
        freeAll();
        return 0;
    }

    scope_pushType(SCOPE_MAIN);
    func_defineBuiltins();

    // Start parsing
    yyparse();

    scope_dump();

    if (compileError) {
        fclose(yyin);
        freeAll();
        return 2;
    }

    if (!stdinMode)
        writeOutputHeader();

    byteBufferWriteToFile(&constBuff, yyout);

    if (!stdinMode)
        writeOutputMain();

    compilerLog("凡%d行\n", yylineno);
    fclose(yyin);

    freeAll();
    return 0;
}
