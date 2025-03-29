#include <gtest/gtest.h>
#include <cstdint>
#include "../ref_list.h"

static const std::size_t PAGE_SIZE = 4096;

class MemoryMapState {
public:
    void *memory_map;
    std::size_t memory_map_size;

    MemoryMapState(std::size_t memory_map_size):
        memory_map_size(memory_map_size) {

        memory_map = new char[memory_map_size];
    }

    ~MemoryMapState() {
        delete[] (char*) memory_map;
    }
};

struct MockBlockDescriptor {
    Object mbd1;
    Object mbd2;
    Ref<MockBlockDescriptor> mbd3;
};

using Node = RefList<int>::Node;

/* MallocTestSeq
 * Tests if Allocator::malloc allocates data sequentially when no "holes" are
 * present in memory.
 */
TEST(RefListTest, PopPushTest) {
    MemoryMapState mm_state(1024);

    current_base = (unsigned char *) mm_state.memory_map;

    auto *list = (RefList<int> *) current_base;
    current_base += sizeof(RefList<int>);

    unsigned char *data = (Ref<unsigned char>) 1;

    // Allocate nodes
    Object node_offsets[3];

    for (int i = 0; i < 3; i++)
        node_offsets[i] = i * sizeof(Node) + 1;

    for (int i = 0; i < 3; i++)
        ((Ref<Node>) node_offsets[i])->val = i + 1;

    list->push(node_offsets[2]);
    list->push(node_offsets[1]);
    list->push(node_offsets[0]);

    for (const auto node_offset : node_offsets) {
        ASSERT_EQ(((Ref<Node>) node_offset)->val, list->pop()->val);
    }
}

TEST(RefListTest, IteratorTest) {
    MemoryMapState mm_state(1024);

    current_base = (unsigned char *) mm_state.memory_map;

    auto *list = (RefList<int> *) current_base;
    current_base += sizeof(RefList<int>);

    unsigned char *data = (Ref<unsigned char>) 1;

    // Allocate nodes
    constexpr size_t NODE_COUNT = 10;
    Object node_offsets[NODE_COUNT];

    for (int i = 0; i < NODE_COUNT; i++) {
        node_offsets[i] = i * sizeof(Node) + 1;
        auto node = (Ref<Node>) node_offsets[i];
        node->val = i + 1;
        node->next = 0; // Initialize next pointer to null
    }

    Node target_nodes[] = {
        {1, node_offsets[2]},
        {2, 0},
        {3, node_offsets[4]},
        {4, 0},
        {5, node_offsets[6]},
        {6, 0},
        {7, node_offsets[8]},
        {8, 0},
        {9, 0},
        {10, 0}
    };

    for (int i = NODE_COUNT - 1; i >= 0; i--)
        list->push(node_offsets[i]);

    auto it = list->get_iterator();
    while (it.get() != 0) {
        if (it.get()->val % 2 == 0) it.remove();
        else it.next();
    }

    for (int i = 0; i < NODE_COUNT; i++) {
        ASSERT_EQ(((Ref<Node>) node_offsets[i])->val, target_nodes[i].val);
        ASSERT_EQ(((Ref<Node>) node_offsets[i])->next, target_nodes[i].next);
    }
}