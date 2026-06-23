//
// Created by WavJaby on 2026/3/2.
//

#include "value_data.h"

#include <string.h>

#include "compiler_util.h"

static bool valueTypeAccepts(ObjectType declared, ObjectType actual) {
    if (declared == OBJECT_TYPE_AUTO || declared == OBJECT_TYPE_STR)
        return true;
    if (declared == OBJECT_TYPE_NUM)
        return ObjectType_isNumber(actual);
    return declared == actual;
}

static Object* cloneValue(const Object* source) {
    Object* copy = cloneStruct(Object, source);
    switch (source->type) {
    case OBJECT_TYPE_STR:
        if (source->value.str)
            copy->value.str = strdup(source->value.str);
        break;
    case OBJECT_TYPE_REGISTER:
        if (source->value.symbol)
            copy->value.symbol = symbol_clone(source->value.symbol);
        break;
    default:
        if (ObjectType_isNumber(source->type) && source->value.number)
            copy->value.number = cloneStruct(ScientificNotation, source->value.number);
        break;
    }
    return copy;
}

static Object makeDefaultValue(ObjectType type) {
    static const ScientificNotation zero = {
        .type = I32, .fraction = 0, .fractionLen = 1, .exp = 0
    };
    switch (type) {
    case OBJECT_TYPE_NUM:
    case OBJECT_TYPE_I32:
    case OBJECT_TYPE_I64:
    case OBJECT_TYPE_F64:
    case OBJECT_TYPE_AUTO:
        return object_createNumber(&zero);
    case OBJECT_TYPE_BOOL:
        return object_createBool(false);
    case OBJECT_TYPE_STR:
        return object_createStrConst("");
    case OBJECT_TYPE_ARRAY:
        return object_createArray();
    default:
        return (Object){.type = OBJECT_TYPE_UNDEFINED};
    }
}

bool object_ValueDataListCreate(ObjectType valueType, const ScientificNotation* count, ValueData* valueData) {
    linkedList_init(&valueData->valueList);
    valueData->valueType = valueType;
    valueData->count = (count != NULL) ? sciToInt32(count) : 1;
    if (valueData->count <= 0) {
        yyerrorf("所宣之數須大於零\n");
        return true;
    }
    return false;
}

bool object_ValueDataListAdd(ValueData* valueData, const Object* obj, const YYLTYPE* tokenLoc) {
    if ((int)valueData->valueList.length >= valueData->count) {
        yyerrorlf("所列之值多於所宣之數\n", tokenLoc);
        return true;
    }
    const ObjectType objValueType = object_getValueType(obj);
    if (valueData->valueType == OBJECT_TYPE_AUTO) {
        valueData->valueType = objValueType;
    } else if (!valueTypeAccepts(valueData->valueType, objValueType)) {
        yyerrorlf("所賦之屬『%s』與所宣之屬『%s』不符\n",
                  tokenLoc, objectType2str[objValueType], objectType2str[valueData->valueType]);
        return true;
    }

    Object* clone = cloneValue(obj);
    linkedList_addp(&valueData->valueList, 0, clone); // freeFlag=0：不自動 free，由 freeA(free) 統一釋放
    return false;
}

bool object_ValueDataListAddDefaults(ValueData* valueData, const YYLTYPE* tokenLoc) {
    while ((int)valueData->valueList.length < valueData->count) {
        Object obj = makeDefaultValue(valueData->valueType);
        if (obj.type == OBJECT_TYPE_UNDEFINED) {
            yyerrorlf("『%s』無預設之值\n", tokenLoc, objectType2str[valueData->valueType]);
            return true;
        }
        if (object_ValueDataListAdd(valueData, &obj, tokenLoc)) {
            object_free(&obj);
            return true;
        }
        object_free(&obj);
    }
    return false;
}

Object* object_ValueDataListPop(ValueData* valueData) {
    if (valueData->valueList.length == 0)
        return NULL;
    LinkedListNode* node = valueData->valueList.head->next;
    Object* obj = node->value;
    linkedList_deleteNode(&valueData->valueList, node);
    return obj;
}

bool object_ValueDataListFree(ValueData* valueData) {
    linkedList_freeA(&valueData->valueList, free);
    return false;
}
