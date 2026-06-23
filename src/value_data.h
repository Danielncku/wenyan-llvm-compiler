//
// Created by WavJaby on 2026/3/2.
//

#ifndef WENYAN_LLVM_VALUE_DATA_H
#define WENYAN_LLVM_VALUE_DATA_H

#include <WJCL/list/wjcl_linked_list.h>

#include "compiler_util.h"
#include "object.h"

/**
 * For single or multiple variable declaration
 */
typedef struct {
    /** LinkedList<@link Object> */
    LinkedList valueList;
    ObjectType valueType;
    // Target count of valueList
    int32_t count;
} ValueData;


/**
 * Create init ValueData
 * @param valueType
 * @param count number of variables to create
 * @param valueType
 * @param valueData ValueData* output
 * @return false if success
 */
bool object_ValueDataListCreate(ObjectType valueType, const ScientificNotation* count, ValueData* valueData);
/**
 * Add Value to ValueData
 * @param valueData ValueData*
 * @param obj Object* output
 * @param tokenLoc
 * @return false if success
 */
bool object_ValueDataListAdd(ValueData* valueData, const Object* obj, const YYLTYPE* tokenLoc);
bool object_ValueDataListAddDefaults(ValueData* valueData, const YYLTYPE* tokenLoc);
/**
 * Pop Value in ValueData
 * @param valueData ValueData*
 * @return Object*
 */
Object* object_ValueDataListPop(ValueData* valueData);

bool object_ValueDataListFree(ValueData* valueData);

#endif //WENYAN_LLVM_VALUE_DATA_H

