//
// Created by Wavjaby on 2026/3/27.
//

#include "while.h"

#include <stdarg.h>
#include <WJCL/string/wjcl_string.h>

#include "lib/code_gen.h"
#include "../compiler_util.h"
#include "scope.h"

bool code_whileLoopStart() {
    compilerLog("> (while loop)\n");

    const ScopeData* scope = scope_pushType(SCOPE_WHILE_LOOP);
    IRTextBuilder ir = irTextBuilder(&ctx->code);

    irEmitBlank(&ir);
    irEmitBranch(&ir, "loop%d.body", scope->id);
    irEmitLabel(&ir, "loop%d.body", scope->id);

    return false;
}

bool code_whileLoopEnd(Object* obj) {
    const ScopeData* scope = scope_peek();
    IRTextBuilder ir = irTextBuilder(&ctx->code);

    irEmitBranch(&ir, "loop%d.body", scope->id);
    irEmitLabel(&ir, "loop%d.exit", scope->id);
    irEmitBlank(&ir);

    scope_dump();
    compilerLog("< (while loop end)\n");
    return false;
}

bool code_break() {
    const ScopeData* loopScope = scope_findNearestLoop();
    if (!loopScope) {
        yyerrorf("乃止當在循環之內\n");
        return true;
    }
    compilerLog("break (loop%d)\n", loopScope->id);
    IRTextBuilder ir = irTextBuilder(&ctx->code);
    irEmitBranch(&ir, "loop%d.exit", loopScope->id);
    return false;
}
