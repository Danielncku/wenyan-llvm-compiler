#ifndef WENYAN_EXPRESSION_H
#define WENYAN_EXPRESSION_H

#include "object.h"

Object code_expression(ExpOp eop, bool opLeft, Object* a, Object* b,
                       const YYLTYPE* aLoc, const YYLTYPE* bLoc);
Object code_expressionMod(ExpOp dop, ExpOp eop, bool op_left, Object* a, Object* b,
                          YYLTYPE* dopLoc, YYLTYPE* eopLoc);
Object code_expressionChain(ExpOp eop, bool op_left, Object* a, Object* b,
                            YYLTYPE* aLoc, YYLTYPE* bLoc);
Object code_expressionChainMod(ExpOp dop, ExpOp eop, bool op_left, Object* a, Object* b,
                               YYLTYPE* dopLoc, YYLTYPE* eopLoc);

#endif
