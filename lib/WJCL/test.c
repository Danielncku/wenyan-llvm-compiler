#define WJCL_LINKED_LIST_IMPLEMENTATION
#define WJCL_HASH_MAP_IMPLEMENTATION

#define MEM_TRACK
#include "memory/wjcl_mem_track.h"

#include "list/wjcl_linked_list.h"
#include "map/wjcl_hash_map.h"

#include <assert.h>

uint32_t hashInt(void* key) {
    return *(int*)key;
}

bool equalsInt(void* key1, void* key2) {
    return *(int*)key1 == *(int*)key2;
}

void onNodeDelete(void* key, void* value) {
    // printf("Deleting node: k=%d v=%d\n", *(int*)key, *(int*)value);
}

void test_linked_list_edge_cases() {
    printf("--- LinkedList Edge Cases ---\n");
    LinkedList list = linkedList_create();
    linkedList_init(&list);

    // 1. Get from empty list
    assert(linkedList_getNode(&list, 0) == NULL);
    assert(linkedList_getNode(&list, 10) == NULL);

    // 2. Large scale add and delete
    const int count = 2000;
    for (int i = 0; i < count; i++) {
        int* v = malloc(sizeof(int));
        *v = i;
        linkedList_addp(&list, 1, v);
    }
    assert(list.length == count);

    // 3. getNode efficiency/correctness (middle, start, end)
    assert(linkedList_nodeVal(int, linkedList_getNode(&list, 0)) == 0);
    assert(linkedList_nodeVal(int, linkedList_getNode(&list, count - 1)) == count - 1);
    assert(linkedList_nodeVal(int, linkedList_getNode(&list, count / 2)) == count / 2);

    // 4. Delete all using safe foreach
    int deletedCount = 0;
    linkedList_foreach_safe(&list, node, n) {
        linkedList_deleteNode(&list, node);
        deletedCount++;
    }
    assert(deletedCount == count);
    assert(list.length == 0);
    assert(list.head->next == list.head);
    assert(list.head->prev == list.head);

    printf("LinkedList edge cases passed\n");
}

void test_map_edge_cases() {
    printf("--- Map Edge Cases ---\n");
    MapNodeInfo info = {equalsInt, hashInt, onNodeDelete, WJCL_HASH_MAP_FREE_KEY | WJCL_HASH_MAP_FREE_VALUE};
    Map* map = map_new(info);

    // 1. Stress test / Rehashing
    const int count = 2000;
    for (int i = 0; i < count; i++) {
        int* k = malloc(sizeof(int));
        int* v = malloc(sizeof(int));
        *k = i;
        *v = i * 10;
        map_putpp(map, k, v);
    }
    assert(map->size == count);
    assert(map->bucketSize > WJCL_HASH_MAP_DEFAULT_CAPACITY);

    // 2. Verify values after rehash
    for (int i = 0; i < count; i++) {
        int k = i;
        void* v = map_get(map, &k);
        if (v == NULL) {
            printf("Failed to find key %d after rehash\n", i);
            assert(false);
        }
        assert(*(int*)v == i * 10);
    }

    // 3. Update existing key
    int* k_upd = malloc(sizeof(int));
    int* v_upd = malloc(sizeof(int));
    *k_upd = 500;
    *v_upd = 9999;
    map_putpp(map, k_upd, v_upd);
    int k_find = 500;
    assert(*(int*)map_get(map, &k_find) == 9999);
    assert(map->size == count);

    // 4. Delete some
    for (int i = 0; i < 500; i++) {
        int k = i;
        map_delete(map, &k);
    }
    assert(map->size == count - 500);

    // Verify remaining
    for (int i = 500; i < count; i++) {
        int k = i;
        void* v = map_get(map, &k);
        assert(v != NULL);
        if (i == 500) assert(*(int*)v == 9999);
        else assert(*(int*)v == i * 10);
    }

    // 5. Clear
    map_clear(map);
    assert(map->size == 0);
    assert(map->bucketSize == WJCL_HASH_MAP_DEFAULT_CAPACITY);

    map_free(map);
    free(map);
    printf("Map edge cases passed\n");
}

uint32_t hashAlwaysZero(void* key) {
    return 0;
}

int g_deleteCount = 0;
void onNodeDeleteTrack(void* key, void* value) {
    g_deleteCount++;
    free(key);
    free(value);
}

void test_map_collisions_and_callbacks() {
    printf("--- Map Collisions and Callbacks ---\n");
    g_deleteCount = 0;
    // Free flag is 0 because we handle it in onNodeDeleteTrack for this specific test
    MapNodeInfo info = {equalsInt, hashAlwaysZero, onNodeDeleteTrack, 0};
    Map* map = map_new(info);

    const int count = 10;
    for (int i = 0; i < count; i++) {
        int* k = malloc(sizeof(int));
        int* v = malloc(sizeof(int));
        *k = i;
        *v = i * 100;
        map_putpp(map, k, v);
    }

    assert(map->size == count);
    assert(map->bukketUsed == 1); // All in bucket 0

    // Test deletion triggers callback
    int k_del = 5;
    map_delete(map, &k_del);
    assert(map->size == count - 1);
    assert(g_deleteCount == 1);

    // Test clear triggers callback for all remaining
    map_clear(map);
    assert(map->size == 0);
    assert(g_deleteCount == count);

    map_free(map);
    free(map);
    printf("Map collisions and callbacks passed\n");
}

void test_linked_list_getNode_edge_cases() {
    printf("--- LinkedList getNode Edge Cases ---\n");
    LinkedList list = linkedList_create();
    linkedList_init(&list);

    // Empty list
    assert(linkedList_getNode(&list, 0) == NULL);

    // One element
    int v1 = 100;
    linkedList_addp(&list, 0, &v1);
    assert(linkedList_getNode(&list, 0) != NULL);
    assert(linkedList_nodeVal(int, linkedList_getNode(&list, 0)) == 100);
    assert(linkedList_getNode(&list, 1) == NULL);

    // Two elements
    int v2 = 200;
    linkedList_addp(&list, 0, &v2);
    assert(linkedList_nodeVal(int, linkedList_getNode(&list, 0)) == 100);
    assert(linkedList_nodeVal(int, linkedList_getNode(&list, 1)) == 200);
    assert(linkedList_getNode(&list, 2) == NULL);

    // Middle access test with 5 elements
    for(int i=3; i<=5; i++) {
        int* v = malloc(sizeof(int));
        *v = i * 100;
        linkedList_addp(&list, 1, v);
    }
    // Indices: 0:100, 1:200, 2:300, 3:400, 4:500
    assert(linkedList_nodeVal(int, linkedList_getNode(&list, 2)) == 300);
    assert(linkedList_nodeVal(int, linkedList_getNode(&list, 4)) == 500);

    linkedList_free(&list);
    printf("LinkedList getNode edge cases passed\n");
}

void test_map_put_update_delete() {
    printf("--- Map Put/Update/Delete Edge Cases ---\n");
    MapNodeInfo info = {equalsInt, hashInt, onNodeDelete, WJCL_HASH_MAP_FREE_KEY | WJCL_HASH_MAP_FREE_VALUE};
    Map* map = map_new(info);

    // Put null key/value (if allowed by logic, though usually not recommended)
    // Current implementation doesn't check for NULL keys in hash function, so we use real pointers
    int* k1 = malloc(sizeof(int)); *k1 = 1;
    int* v1 = malloc(sizeof(int)); *v1 = 10;
    map_putpp(map, k1, v1);

    // Update with same key pointer but different value
    int* v1_new = malloc(sizeof(int)); *v1_new = 20;
    map_putpp(map, k1, v1_new); // This should free OLD v1 and keep k1
    assert(*(int*)map_get(map, k1) == 20);
    assert(map->size == 1);

    // Update with DIFFERENT key pointer but EQUAL key value
    int* k1_new = malloc(sizeof(int)); *k1_new = 1;
    int* v1_new2 = malloc(sizeof(int)); *v1_new2 = 30;
    map_putpp(map, k1_new, v1_new2); // This should free OLD k1 and OLD v1_new
    assert(*(int*)map_get(map, k1_new) == 30);
    assert(map->size == 1);

    map_free(map);
    free(map);
    printf("Map Put/Update/Delete edge cases passed\n");
}

void test_linked_list_macros() {
    printf("--- LinkedList Macros ---\n");
    LinkedList list = linkedList_create();
    linkedList_init(&list);

    // Test linkedList_add
    linkedList_add(&list, 10);
    linkedList_add(&list, 20);
    linkedList_add(&list, 30);

    assert(list.length == 3);
    assert(linkedList_get(&list, int, 0) == 10);
    assert(linkedList_get(&list, int, 1) == 20);
    assert(linkedList_get(&list, int, 2) == 30);

    // Test linkedList_addPtr
    int v4 = 40;
    linkedList_addPtr(&list, &v4);
    assert(list.length == 4);
    assert(*(int*)linkedList_getPtr(&list, 3) == 40);

    // Test linkedList_foreach
    int sum = 0;
    linkedList_foreach(&list, node) {
        sum += linkedList_nodeVal(int, node);
    }
    assert(sum == 100);

    // Test linkedList_foreach_safe and linkedList_clear
    int count = 0;
    linkedList_foreach_safe(&list, node, n) {
        count++;
    }
    assert(count == 4);

    linkedList_clear(&list);
    assert(list.length == 0);

    linkedList_free(&list);
    printf("LinkedList macros passed\n");
}

void test_map_macros() {
    printf("--- Map Macros ---\n");
    
    // Test map_create
    Map map = map_create(equalsInt, hashInt, NULL, WJCL_HASH_MAP_FREE_KEY | WJCL_HASH_MAP_FREE_VALUE);
    
    for (int i = 0; i < 5; i++) {
        int* k = malloc(sizeof(int));
        int* v = malloc(sizeof(int));
        *k = i;
        *v = i * 100;
        map_putpp(&map, k, v);
    }

    // Test map_entries
    int count = 0;
    int sumK = 0;
    int sumV = 0;
    map_entries(&map, entry) {
        sumK += *(int*)entry->key;
        sumV += *(int*)entry->value;
        count++;
    }

    assert(count == 5);
    assert(sumK == 10); // 0+1+2+3+4
    assert(sumV == 1000); // 0+100+200+300+400

    map_free(&map);

    // Test map_createFromInfo
    MapNodeInfo info = {equalsInt, hashInt, NULL, 0};
    Map map2 = map_createFromInfo(info);
    int k2 = 1, v2 = 2;
    map_putpp(&map2, &k2, &v2);
    assert(map2.size == 1);
    assert(*(int*)map_get(&map2, &k2) == 2);
    map_free(&map2);

    printf("Map macros passed\n");
}

int main(int argc, char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    test_linked_list_edge_cases();
    test_linked_list_getNode_edge_cases();
    test_linked_list_macros();
    test_map_edge_cases();
    test_map_collisions_and_callbacks();
    test_map_put_update_delete();
    test_map_macros();

    printf("\nAll expanded tests passed successfully.\n");
    fflush(stdout);
    memTrackResult();
    return 0;
}
