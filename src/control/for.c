//
// Created by WavJaby on 2026/03/26.
//

#include "for.h"

#include <stdarg.h>
#include <WJCL/string/wjcl_string.h>

#include "lib/code_gen.h"
#include "compiler_util.h"
#include "scope.h"

bool code_forLoop(Object* src) {
    if (src->type == OBJECT_TYPE_UNDEFINED)
        goto FAILED;

    compilerLog("> (for loop, count: %s)\n", object_print(src));

    ScopeData* scope = scope_pushType(SCOPE_FOR_LOOP);
    scope->u.forLoop.symbol = (SymbolData){
        .type = OBJECT_TYPE_I32,
        .elementType = OBJECT_TYPE_UNDEFINED,
        .name = strdup("i"),
        .index = -1,
        .funcInfo = NULL,
        .funcArg = false,
    };

    char countName[MAX_NAME_LENGTH];
    Object regSrc = object_loadRegAndPromote(src, OBJECT_TYPE_I32, countName, MAX_NAME_LENGTH);
    if (regSrc.type == OBJECT_TYPE_UNDEFINED)
        goto FAILED;

    const int id = scope->id;
    IRTextBuilder ir = irTextBuilder(&ctx->code);
    irEmitBlank(&ir);
    irEmitBranch(&ir, "loop%d.entry", id);
    irEmitLabel(&ir, "loop%d.entry", id);
    irEmitBranch(&ir, "loop%d.header", id);
    irEmitLabel(&ir, "loop%d.header", id);
    irEmit(&ir, "%%loop%d.i = phi i32 [ 0, %%loop%d.entry ], [ %%loop%d.i.next, %%loop%d.update ]",
           id, id, id, id);
    irEmit(&ir, "%%loop%d.cond = icmp slt i32 %%loop%d.i, %s", id, id, countName);

    char condition[MAX_NAME_LENGTH];
    char bodyLabel[MAX_NAME_LENGTH];
    char exitLabel[MAX_NAME_LENGTH];
    snprintf(condition, sizeof(condition), "%%loop%d.cond", id);
    snprintf(bodyLabel, sizeof(bodyLabel), "loop%d.body", id);
    snprintf(exitLabel, sizeof(exitLabel), "loop%d.exit", id);
    irEmitConditionalBranch(&ir, condition, bodyLabel, exitLabel);
    irEmitLabel(&ir, "loop%d.body", id);

    if (src->type == OBJECT_TYPE_SYMBOL || (regSrc.type == OBJECT_TYPE_REGISTER && src->type != OBJECT_TYPE_REGISTER))
        object_free(&regSrc);
    object_free(src);
    return false;

FAILED:
    object_free(src);
    return true;
}

bool code_forLoopEnd(Object* obj) {
    const ScopeData* scope = scope_peek();
    const int id = scope->id;
    const char* typeName = objectType2llvmType[scope->u.forLoop.symbol.type];
    IRTextBuilder ir = irTextBuilder(&ctx->code);
    irEmitBranch(&ir, "loop%d.update", id);
    irEmitLabel(&ir, "loop%d.update", id);
    irEmit(&ir, "%%loop%d.i.next = add nsw %s %%loop%d.i, 1", id, typeName, id);
    irEmitBranch(&ir, "loop%d.header", id);
    irEmitLabel(&ir, "loop%d.exit", id);
    irEmitBlank(&ir);

    scope_dump();
    compilerLog("< (for loop end)\n");
    return false;
}
