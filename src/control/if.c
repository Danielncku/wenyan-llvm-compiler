//
// Created by Wavjaby on 2026/3/26.
//

#include "if.h"

#include <stdarg.h>
#include <WJCL/string/wjcl_string.h>

#include "lib/code_gen.h"
#include "compiler_util.h"
#include "scope.h"

static void makeIfLabel(char* buffer, size_t size, int id, const char* suffix) {
    snprintf(buffer, size, "if%d.%s", id, suffix);
}

static void makeElseIfLabel(char* buffer, size_t size, int id, int branch, const char* suffix) {
    snprintf(buffer, size, "if%d.elseif%d.%s", id, branch, suffix);
}

inline bool code_if(Object* src) {
    compilerLog("> (if)\n");
    char condName[MAX_NAME_LENGTH];
    Object cond = object_loadRegAndPromote(src, OBJECT_TYPE_BOOL, condName, MAX_NAME_LENGTH);
    if (cond.type == OBJECT_TYPE_UNDEFINED) {
        object_free(src);
        return true;
    }

    ScopeData* scope = scope_pushType(SCOPE_IF_STMT);
    scope->u.ifInfo = (IfInfo){.elseifCount = 0, .containsElse = false};
    IRTextBuilder ir = irTextBuilder(&ctx->code);
    char trueLabel[MAX_NAME_LENGTH], falseLabel[MAX_NAME_LENGTH];
    makeIfLabel(trueLabel, sizeof(trueLabel), scope->id, "true");
    makeIfLabel(falseLabel, sizeof(falseLabel), scope->id, "false");
    irEmitConditionalBranch(&ir, condName, trueLabel, falseLabel);
    irEmitLabel(&ir, "%s", trueLabel);

    if (src->type == OBJECT_TYPE_SYMBOL || (cond.type == OBJECT_TYPE_REGISTER && src->type != OBJECT_TYPE_REGISTER))
        object_free(&cond);
    object_free(src);
    return false;
}

inline bool code_elseIfLabel() {
    ScopeData* scope = scope_peek();
    const int id = scope->id;
    const int count = scope->u.ifInfo.elseifCount;
    IRTextBuilder ir = irTextBuilder(&ctx->code);
    irEmitBranch(&ir, "if%d.endif", id);
    if (count == 0)
        irEmitLabel(&ir, "if%d.false", id);
    else
        irEmitLabel(&ir, "if%d.elseif%d.false", id, count - 1);
    return false;
}

inline bool code_elseIf(Object* src) {
    ScopeData* oldScope = scope_peek();
    const int id = oldScope->id;
    const int count = oldScope->u.ifInfo.elseifCount;
    const bool containsElse = oldScope->u.ifInfo.containsElse;
    scope_dump();

    char condName[MAX_NAME_LENGTH];
    Object cond = object_loadRegAndPromote(src, OBJECT_TYPE_BOOL, condName, MAX_NAME_LENGTH);
    if (cond.type == OBJECT_TYPE_UNDEFINED) {
        object_free(src);
        return true;
    }

    compilerLog("> (else if)\n");
    ScopeData* scope = scope_pushId(SCOPE_IF_STMT, id);
    scope->u.ifInfo = (IfInfo){.elseifCount = count + 1, .containsElse = containsElse};
    IRTextBuilder ir = irTextBuilder(&ctx->code);
    char trueLabel[MAX_NAME_LENGTH], falseLabel[MAX_NAME_LENGTH];
    makeElseIfLabel(trueLabel, sizeof(trueLabel), id, count, "true");
    makeElseIfLabel(falseLabel, sizeof(falseLabel), id, count, "false");
    irEmitConditionalBranch(&ir, condName, trueLabel, falseLabel);
    irEmitLabel(&ir, "%s", trueLabel);

    if (src->type == OBJECT_TYPE_SYMBOL || (cond.type == OBJECT_TYPE_REGISTER && src->type != OBJECT_TYPE_REGISTER))
        object_free(&cond);
    object_free(src);
    return false;
}

inline bool code_else() {
    ScopeData* oldScope = scope_peek();
    const int id = oldScope->id;
    const int count = oldScope->u.ifInfo.elseifCount;
    scope_dump();
    compilerLog("> (else)\n");

    IRTextBuilder ir = irTextBuilder(&ctx->code);
    irEmitBranch(&ir, "if%d.endif", id);
    if (count == 0)
        irEmitLabel(&ir, "if%d.false", id);
    else
        irEmitLabel(&ir, "if%d.elseif%d.false", id, count - 1);

    ScopeData* scope = scope_pushId(SCOPE_IF_STMT, id);
    scope->u.ifInfo = (IfInfo){.elseifCount = count, .containsElse = true};
    return false;
}

inline bool code_ifEnd() {
    ScopeData* scope = scope_peek();
    const int id = scope->id;
    const int count = scope->u.ifInfo.elseifCount;
    const bool hasElse = scope->u.ifInfo.containsElse;
    IRTextBuilder ir = irTextBuilder(&ctx->code);

    if (hasElse) {
        irEmitBranch(&ir, "if%d.endif", id);
        irEmitLabel(&ir, "if%d.endif", id);
    } else if (count == 0) {
        irEmitBranch(&ir, "if%d.false", id);
        irEmitLabel(&ir, "if%d.false", id);
    } else {
        irEmitBranch(&ir, "if%d.endif", id);
        irEmitLabel(&ir, "if%d.elseif%d.false", id, count - 1);
        irEmitBranch(&ir, "if%d.endif", id);
        irEmitLabel(&ir, "if%d.endif", id);
    }
    scope_dump();
    compilerLog("< (if end)\n");
    return false;
}
