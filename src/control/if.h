//
// Created by Wavjaby on 2026/3/26.
//

#ifndef WENYAN_LLVM_IF_H
#define WENYAN_LLVM_IF_H

#include "object.h"

typedef struct {
    int elseifCount;
    bool containsElse;
} IfInfo;

bool code_if(Object* src);
bool code_elseIfLabel();
bool code_elseIf(Object* src);
bool code_else();
bool code_ifEnd();

#endif //WENYAN_LLVM_IF_H
