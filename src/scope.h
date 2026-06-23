//
// Created by WavJaby on 2026/3/26.
//

#ifndef WENYAN_LLVM_SCOPE_H
#define WENYAN_LLVM_SCOPE_H

#include <WJCL/map/wjcl_hash_map.h>
#include "lib/byte_buffer.h"
#include "object.h"
#include "control/for.h"
#include "control/if.h"
#include "control/while.h"

typedef enum {
    SCOPE_MAIN,
    SCOPE_FOR_LOOP,
    SCOPE_WHILE_LOOP,
    SCOPE_IF_STMT,
    SCOPE_FUNCTION,
} ScopeType;

extern const char* ScopeType_toString[];

typedef struct {
    /** Map<char*, @link SymbolData> */
    Map symbolMap;

    ScopeType type;
    int id; // Unique id in context

    union {
        LoopInfo forLoop;
        WhileInfo whileLoop;
        IfInfo ifInfo;
        // borrow，owned by outer symbolMap
        SymbolData* funcSymbol;
    } u;
} ScopeData;

typedef struct CompilerContext {
    int id; // Globally unique context ID, assigned at context_push()
    int variableCount;
    int registerCount;
    int scopeCount; // Track scope count in this context
    bool hasReturn;
    /** LinkedList<@link ScopeData> */
    LinkedList scopeStack;

    int currentLevel;
    ByteBuffer code;

    Object last_result;

    struct CompilerContext* prev;
    struct CompilerContext* next;
} CompilerContext;

extern CompilerContext* ctx;
extern CompilerContext mainCtx;
extern const MapNodeInfo capturedMapInfo;

void scope_init();

ScopeData* scope_pushType(ScopeType type);
ScopeData* scope_pushId(ScopeType type, int scopeId);
void scope_dump();
ScopeData* scope_peek();

SymbolData* scope_addSymbol(ObjectType type, char* name);
/** Returns a borrowed reference into symbolMap or capturedSymbolMap. NO free needed. */
Object scope_findSymbol(char* name);

ScopeData* scope_findNearestLoop();
ScopeData* scope_getFunction();

void context_push();
void context_pop();

void scope_free_all();

#endif //WENYAN_LLVM_SCOPE_H
