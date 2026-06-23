//
// Created by WavJaby on 2026/5/25.
//

#ifndef WENYAN_LLVM_FUNCTION_INFO_H
#define WENYAN_LLVM_FUNCTION_INFO_H

#include <WJCL/list/wjcl_linked_list.h>
#include <WJCL/map/wjcl_hash_map.h>

#include "object_type.h"

typedef struct SymbolData SymbolData;

typedef struct {
    SymbolData* symbol; // Borrowed
    int capturedIndex; // Field offset in closure
} CapturedEntry;

typedef struct {
    char* baseName;

    int paramCount;
    int variantCount;
    const ObjectType** variants; // variants[i][j] = the j-th parameter type in the i-th variant
} FuncInfoBuiltin;

typedef struct {
    int contextId; // ID of the CompilerContext created for this function; used for LLVM label @funcN
    ObjectType returnType;
    /** LinkedList<@link SymbolData>
     * Borrowed until func_defineBodyEnd, then replaced with owned clones (deep-copied)
     */
    LinkedList params;
    /** LinkedList<@link CapturedEntry*>
     * Owned
     */
    LinkedList capturedVars;

    /** Map<char*, @link CapturedEntry*>
     * Owned
     */
    Map capturedSymbolMap;

    FuncInfoBuiltin* builtin;
} FuncInfo;

typedef struct {
    FuncInfo* func;
    /**
     * LinkedList<@link Object>
     * Owned
     */
    LinkedList args;

    /**
     * LinkedListNode<@link SymbolData>
     * Borrowed from func->params, for type checking
     */
    LinkedListNode* currentArg;
} FuncCallInfo;

#endif //WENYAN_LLVM_FUNCTION_INFO_H
