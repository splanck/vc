#include <stdio.h>
#include "semantic_global.h"
#include <stdint.h>

/* Minimal copies of the packing helpers to avoid linking the full compiler */
size_t semantic_pack_alignment = 0;
void semantic_set_pack(size_t align) { semantic_pack_alignment = align; }

size_t layout_struct_members(struct_member_t *members, size_t count)
{
    size_t byte_off = 0;
    unsigned bit_off = 0;
    size_t pack = semantic_pack_alignment ? semantic_pack_alignment : SIZE_MAX;

    for (size_t i = 0; i < count; i++) {
        if (members[i].bit_width == 0) {
            size_t align = members[i].elem_size;
            if (align > pack)
                align = pack;
            if (bit_off)
                byte_off++, bit_off = 0;
            if (align > 1) {
                size_t rem = byte_off % align;
                if (rem)
                    byte_off += align - rem;
            }
            members[i].offset = byte_off;
            members[i].bit_offset = 0;
            if (!members[i].is_flexible)
                byte_off += members[i].elem_size;
        } else {
            members[i].offset = byte_off;
            members[i].bit_offset = bit_off;
            bit_off += members[i].bit_width;
            byte_off += bit_off / 8;
            bit_off %= 8;
        }
    }

    if (bit_off)
        byte_off++;
    if (pack != SIZE_MAX && pack > 1) {
        size_t rem = byte_off % pack;
        if (rem)
            byte_off += pack - rem;
    }
    return byte_off;
}

static int failures;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

static void test_pack2(void)
{
    struct_member_t mems[2] = {
        {"a", TYPE_CHAR, 1, 0, 0, 0, 0},
        {"b", TYPE_INT, 4, 0, 0, 0, 0}
    };
    semantic_set_pack(2);
    size_t sz = layout_struct_members(mems, 2);
    ASSERT(mems[1].offset == 2);
    ASSERT(sz == 6);
}

static void test_pack4(void)
{
    struct_member_t mems[2] = {
        {"a", TYPE_CHAR, 1, 0, 0, 0, 0},
        {"b", TYPE_INT, 4, 0, 0, 0, 0}
    };
    semantic_set_pack(4);
    size_t sz = layout_struct_members(mems, 2);
    ASSERT(mems[1].offset == 4);
    ASSERT(sz == 8);
}

int main(void)
{
    test_pack2();
    test_pack4();
    if (failures == 0)
        printf("All pack_pragma tests passed\n");
    else
        printf("%d pack_pragma test(s) failed\n", failures);
    return failures ? 1 : 0;
}
