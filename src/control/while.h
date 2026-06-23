//
// Created by Wavjaby on 2026/3/27.
//

#ifndef WENYAN_LLVM_WHILE_H
#define WENYAN_LLVM_WHILE_H

#include "object.h"

typedef struct {
} WhileInfo;

bool code_whileLoopStart();
bool code_whileLoopEnd(Object* obj);
bool code_break();

#endif //WENYAN_LLVM_WHILE_H
