//
// Created by WavJaby on 2026/3/26.
//

#include "scope.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <WJCL/string/wjcl_string.h>
#include <WJCL/map/wjcl_hash_map.h>

#include "lib/code_gen.h"
#include "lib/console_color.h"
#include "compiler_util.h"
#include "object.h"
#include "control/for.h"
#include "control/if.h"

const char* ScopeType_toString[] = {
    [SCOPE_MAIN] = "SCOPE_MAIN",
    [SCOPE_FOR_LOOP] = "SCOPE_FOR_LOOP",
    [SCOPE_WHILE_LOOP] = "SCOPE_WHILE_LOOP",
    [SCOPE_IF_STMT] = "SCOPE_IF_STMT",
    [SCOPE_FUNCTION] = "SCOPE_FUNCTION",
};

CompilerContext* ctx;

static int contextCount = 1; // 0 is reserved for mainCtx

CompilerContext mainCtx = {
    .id = 0,
    .variableCount = 0, .registerCount = 0,
    .scopeStack = linkedList_create(),
    .code = byteBufferInit(),
    .currentLevel = 0,
    .last_result = {.type = OBJECT_TYPE_UNDEFINED},
    .prev = NULL,
    .next = NULL
};

bool symbolKeyEquals(void* key1, void* key2) {
    return strcmp(key1, key2) == 0;
}

uint32_t symbolKeyHash(void* key) {
    return strHash(key);
}

const MapNodeInfo capturedMapInfo = {
    symbolKeyEquals, symbolKeyHash, NULL,
    WJCL_HASH_MAP_FREE_KEY | WJCL_HASH_MAP_FREE_VALUE
};

void symbolValueFree(void* key, void* value) {
    // freeSelf will also free value pointer, dont need WJCL_HASH_MAP_FREE_VALUE
    symbol_freeSelf(value);
}

static const MapNodeInfo symbolMapInfo = {
    symbolKeyEquals, symbolKeyHash, symbolValueFree,
    WJCL_HASH_MAP_FREE_KEY
};


void scope_init() {
    ctx = &mainCtx;

    linkedList_init(&mainCtx.scopeStack);
}

ScopeData* scope_pushType(const ScopeType type) {
    return scope_pushId(type, ctx->scopeCount++);
}

ScopeData* scope_pushId(const ScopeType type, const int scopeId) {
    compilerLog("> (scope id: %d, type: %s)\n", scopeId, ScopeType_toString[type]);

    const ScopeData scopeData = {
        .symbolMap = (Map)map_createFromInfo(symbolMapInfo),
        .type = type,
        .id = scopeId,
    };
    linkedList_addp(&ctx->scopeStack, true, cloneStruct(ScopeData, &scopeData));
    ++ctx->currentLevel;
    return ctx->scopeStack.head->prev->value;
}

void scope_push_typed(ScopeType type, const void* data) {
    compilerLog("> (scope level %d, type %s)\n", ctx->currentLevel, ScopeType_toString[type]);

    ScopeData scopeData = (ScopeData){
        .symbolMap = (Map)map_createFromInfo(symbolMapInfo),
        .type = type,
    };

    if (data) {
        switch (type) {
        case SCOPE_FOR_LOOP:
            scopeData.u.forLoop = *(const LoopInfo*)data;
            break;
        case SCOPE_IF_STMT:
            scopeData.u.ifInfo = *(const IfInfo*)data;
            break;
        case SCOPE_WHILE_LOOP:
            scopeData.u.whileLoop = *(const WhileInfo*)data;
            break;
        default:
            break;
        }
    }

    linkedList_addp(&ctx->scopeStack, true, cloneStruct(ScopeData, &scopeData));
    ++ctx->currentLevel;
}

// cloneStruct / map_putpp / strdup 用法：見 README.md §工具函式速查
SymbolData* scope_addSymbol(const ObjectType type, char* name) {
    linkedList_foreach(&ctx->scopeStack, node) {
        ScopeData* scopeData = node->value;
        if (map_get(&scopeData->symbolMap, name)) {
            yyerrorf("「%s」已嘗立名，不可復立\n", name);
            return NULL;
        }
    }

    const SymbolData symbol = {
        .type = type,
        .elementType = OBJECT_TYPE_UNDEFINED,
        .name = strdup(name),
        .index = ctx->variableCount++,
        .funcInfo = NULL,
        .funcArg = false,
    };
    SymbolData* symbolData = cloneStruct(SymbolData, &symbol);
    if (!symbolData)
        exit(EXIT_FAILURE);

    ScopeData* _currentScope = ctx->scopeStack.head->prev->value;
    map_putpp(&_currentScope->symbolMap, strdup(name), symbolData);
    return symbolData;
}

static int symCmp(const void* a, const void* b) {
    const SymbolData* sa = *(const SymbolData**)a;
    const SymbolData* sb = *(const SymbolData**)b;
    if (sa->index != sb->index) return sa->index - sb->index;
    return strcmp(sa->name, sb->name);
}

static void scope_printSymbolTable(const ScopeData* scopeData) {
    // Column headers
    static const char* H_NAME  = "名稱";
    static const char* H_TYPE  = "型別";
    static const char* H_INDEX = "Index";
    static const char* H_FUNC  = "術";

    // Minimum column widths from header
    int nameW  = calculateStringWidthUtf8(H_NAME);
    const int typeW  = calculateStringWidthUtf8(H_TYPE);
    const int indexW = calculateStringWidthUtf8(H_INDEX);
    int funcW  = calculateStringWidthUtf8(H_FUNC);

    // Measure widest name and func cell
    map_entries(&scopeData->symbolMap, entry) {
        const SymbolData* sym = entry->value;
        const int w = calculateStringWidthUtf8(sym->name);
        if (w > nameW) nameW = w;

        if (sym->funcInfo) {
            char funcBuf[16];
            if (sym->funcInfo->builtin)
                snprintf(funcBuf, sizeof(funcBuf), "builtin");
            else
                snprintf(funcBuf, sizeof(funcBuf), "ctx:%d", sym->funcInfo->contextId);
            const int fw = calculateStringWidthUtf8(funcBuf);
            if (fw > funcW) funcW = fw;
        }
    }

    // Box-drawing helpers: each cell padded to column width
#define BOX_ROW(lc, mc, rc, fill) do {                          \
    fprintf(stdout, "%s%s", COLOR_CYAN, lc);                    \
    for (int i = 0; i < nameW  + 2; i++) fprintf(stdout, fill); \
    fprintf(stdout, mc);                                        \
    for (int i = 0; i < typeW  + 2; i++) fprintf(stdout, fill); \
    fprintf(stdout, mc);                                        \
    for (int i = 0; i < indexW + 2; i++) fprintf(stdout, fill); \
    fprintf(stdout, mc);                                        \
    for (int i = 0; i < funcW  + 2; i++) fprintf(stdout, fill); \
    fprintf(stdout, "%s%s\n", rc, COLOR_RESET);                 \
} while (0)

#define BOX_CELL(val, width) do {                               \
    const int w = calculateStringWidthUtf8(val);                \
    fprintf(stdout, " %s%*s", val, width - w + 1, "");          \
} while (0)

    // Top border
    BOX_ROW("╔", "╦", "╗", "═");

    // Header row
    fprintf(stdout, "%s║%s", COLOR_CYAN, COLOR_RESET);
    BOX_CELL(H_NAME,  nameW);
    fprintf(stdout, "%s║%s", COLOR_CYAN, COLOR_RESET);
    BOX_CELL(H_TYPE,  typeW);
    fprintf(stdout, "%s║%s", COLOR_CYAN, COLOR_RESET);
    BOX_CELL(H_INDEX, indexW);
    fprintf(stdout, "%s║%s", COLOR_CYAN, COLOR_RESET);
    BOX_CELL(H_FUNC,  funcW);
    fprintf(stdout, "%s║%s\n", COLOR_CYAN, COLOR_RESET);

    // Header separator
    BOX_ROW("╠", "╬", "╣", "═");

    // Collect and sort entries by index (tiebreak: name strcmp)
    const size_t symCount = scopeData->symbolMap.size;
    const SymbolData** syms = malloc(symCount * sizeof(SymbolData*));
    size_t symIdx = 0;
    map_entries(&scopeData->symbolMap, entry) {
        syms[symIdx++] = entry->value;
    }
    qsort(syms, symCount, sizeof(SymbolData*), symCmp);

    // Data rows
    for (size_t i = 0; i < symCount; i++) {
        const SymbolData* sym = syms[i];
        char indexBuf[16];
        snprintf(indexBuf, sizeof(indexBuf), "%d", sym->index);

        char funcBuf[16] = "";
        if (sym->funcInfo) {
            if (sym->funcInfo->builtin)
                snprintf(funcBuf, sizeof(funcBuf), "builtin");
            else
                snprintf(funcBuf, sizeof(funcBuf), "ctx:%d", sym->funcInfo->contextId);
        }

        fprintf(stdout, "%s║%s", COLOR_CYAN, COLOR_RESET);
        BOX_CELL(sym->name, nameW);
        fprintf(stdout, "%s║%s", COLOR_CYAN, COLOR_RESET);
        BOX_CELL(objectType2str[sym->type] ? objectType2str[sym->type] : "?", typeW);
        fprintf(stdout, "%s║%s", COLOR_CYAN, COLOR_RESET);
        BOX_CELL(indexBuf, indexW);
        fprintf(stdout, "%s║%s", COLOR_CYAN, COLOR_RESET);
        BOX_CELL(funcBuf, funcW);
        fprintf(stdout, "%s║%s\n", COLOR_CYAN, COLOR_RESET);
    }
    free(syms);

    // Bottom border
    BOX_ROW("╚", "╩", "╝", "═");

#undef BOX_ROW
#undef BOX_CELL
}

void scope_dump() {
    --ctx->currentLevel;
    ScopeData* scopeData = ctx->scopeStack.head->prev->value;

    compilerLog("< (scope id: %d, type: %s)\n", scopeData->id, ScopeType_toString[scopeData->type]);

    if (verbose && scopeData->symbolMap.size > 0)
        scope_printSymbolTable(scopeData);

    map_free(&scopeData->symbolMap);

    linkedList_deleteNode(&ctx->scopeStack, ctx->scopeStack.head->prev);
}

ScopeData* scope_peek() {
    if (!ctx->scopeStack.head) return NULL;
    return ctx->scopeStack.head->prev->value;
}

static ScopeData* scope_getFunctionIn(const CompilerContext* targetCtx) {
    if (!targetCtx->scopeStack.head) return NULL;
    const LinkedListNode* first = targetCtx->scopeStack.head->next;
    if (first == targetCtx->scopeStack.head) return NULL;
    ScopeData* scope = first->value;
    return scope->type == SCOPE_FUNCTION ? scope : NULL;
}

// Add CapturedEntry to funcInfo->capturedSymbolMap.
static CapturedEntry* scope_addCaptured(FuncInfo* funcInfo, SymbolData* symbol) {
    if (symbol->type == OBJECT_TYPE_FUNC && symbol->funcInfo && symbol->funcInfo->builtin) return NULL;

    // Check if already captured
    CapturedEntry* existing = map_get(&funcInfo->capturedSymbolMap, symbol->name);
    if (existing) return existing;

    // Create CapturedEntry (borrow symbol)
    CapturedEntry* entry = malloc(sizeof(CapturedEntry));
    entry->symbol = symbol;
    entry->capturedIndex = (int)funcInfo->capturedVars.length;

    map_putpp(&funcInfo->capturedSymbolMap, strdup(symbol->name), entry);
    linkedList_addp(&funcInfo->capturedVars, false, entry);
    return entry;
}

Object scope_findSymbol(char* name) {
    for (CompilerContext* searchCtx = ctx; searchCtx; searchCtx = searchCtx->prev) {
        linkedList_foreach(&searchCtx->scopeStack, node) {
            ScopeData* scopeData = node->value;
            SymbolData* symbol = map_get(&scopeData->symbolMap, name);
            if (!symbol) continue;

            // TODO: 若 searchCtx == ctx，表示符號在當前 context 內
            //   回傳一個 capturedIndex = -1 的 SYMBOL Object（參考閉包分支的回傳格式）
            if (searchCtx == ctx) {
                return (Object){OBJECT_TYPE_SYMBOL, .capturedIndex = -1, .value.symbol = symbol};
            }

            // Found in an outer function, propagate capture layer by layer.
            // Each layer registers a CapturedEntry pointing to the original SymbolData*.
            CapturedEntry* entry = NULL;
            for (CompilerContext* c = searchCtx->next; c != NULL; c = c->next) {
                ScopeData* funcScope = scope_getFunctionIn(c);
                if (!funcScope) continue;

                FuncInfo* funcInfo = funcScope->u.funcSymbol->funcInfo;
                entry = scope_addCaptured(funcInfo, symbol);
                if (!entry) break; // global symbols, use directly
            }

            // global symbols, use directly
            if (!entry)
                return (Object){OBJECT_TYPE_SYMBOL, .capturedIndex = -1, .value.symbol = symbol};

            // entry is the innermost CapturedEntry; capturedIndex is the caller's upvalue index.
            return (Object){OBJECT_TYPE_SYMBOL, .capturedIndex = entry->capturedIndex, .value.symbol = entry->symbol};
        }
    }

    yyerrorf("「%s」未嘗立名，無由識之\n", name);
    return (Object){OBJECT_TYPE_UNDEFINED, .value = {}};
}

ScopeData* scope_findNearestLoop() {
    if (!ctx->scopeStack.head) return NULL;
    const LinkedListNode* node = ctx->scopeStack.head->prev;
    while (node != ctx->scopeStack.head) {
        ScopeData* scopeData = node->value;
        if (scopeData->type == SCOPE_FOR_LOOP || scopeData->type == SCOPE_WHILE_LOOP)
            return scopeData;
        node = node->prev;
    }
    return NULL;
}

ScopeData* scope_getFunction() {
    return scope_getFunctionIn(ctx);
}

void context_push() {
    CompilerContext* newCtx = malloc(sizeof(CompilerContext));
    *newCtx = (CompilerContext){
        .id = contextCount++,
        .variableCount = 0,
        .registerCount = 0,
        .scopeCount = 0,
        .scopeStack = linkedList_create(),
        .currentLevel = 0,
        .code = byteBufferInit(),
        .last_result = {.type = OBJECT_TYPE_UNDEFINED},
        .prev = ctx,
        .next = NULL
    };
    ctx->next = newCtx;
    linkedList_init(&newCtx->scopeStack);
    ctx = newCtx;
}

void context_pop() {
    CompilerContext* oldCtx = ctx;
    ctx = oldCtx->prev;
    if (ctx) ctx->next = NULL;

    // Free the old context
    byteBufferFree(&oldCtx->code, false);
    linkedList_free(&oldCtx->scopeStack);
    free(oldCtx);
}

void scope_free_all() {
    // Free all scope
    linkedList_foreach(&ctx->scopeStack, node) {
        const ScopeData* scopeData = node->value;
        map_free(&scopeData->symbolMap);
    }
    linkedList_free(&ctx->scopeStack);
}
