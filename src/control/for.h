//
// Created by WavJaby on 2026/03/26.
//

#ifndef WENYAN_LLVM_FOR_H
#define WENYAN_LLVM_FOR_H

#include "object.h"

typedef struct {
    SymbolData symbol;
} LoopInfo;

bool code_forLoop(Object* src);
bool code_forLoopEnd(Object* obj);

#endif //WENYAN_LLVM_FOR_H
