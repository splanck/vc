/*
 * Shared layout helpers used by semantic analysis.
 * Implements algorithms for assigning member offsets and
 * duplicating struct/union metadata.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include "semantic_layout.h"
#include "util.h"
#include "error.h"

/* Active struct packing alignment (0 means natural) */
extern size_t semantic_pack_alignment;

/*
 * Lay out union members sequentially and return the size of the largest
 * member. Offsets are assigned in declaration order.
 */
size_t layout_union_members(union_member_t *members, size_t count)
{
    size_t max = 0;
    for (size_t i = 0; i < count; i++) {
        members[i].offset = 0;
        members[i].bit_offset = 0;
        if (members[i].elem_size > max)
            max = members[i].elem_size;
    }
    return max;
}

/*
 * Compute byte offsets for struct members sequentially and return the
 * total size of the struct. Packing is honoured via semantic_pack_alignment.
 */
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

/* Compute layout for a union variable declaration */
int compute_union_layout(stmt_t *decl, symtable_t *globals)
{
    if (STMT_VAR_DECL(decl).member_count) {
        size_t max = layout_union_members(STMT_VAR_DECL(decl).members,
                                          STMT_VAR_DECL(decl).member_count);
        STMT_VAR_DECL(decl).elem_size = max;
    } else if (STMT_VAR_DECL(decl).tag) {
        symbol_t *utype = symtable_lookup_union(globals, STMT_VAR_DECL(decl).tag);
        if (!utype) {
            error_set(decl->line, decl->column, error_current_file,
                      error_current_function);
            return 0;
        }
        STMT_VAR_DECL(decl).elem_size = utype->total_size;
    }
    return 1;
}

/* Compute layout for a struct variable declaration */
int compute_struct_layout(stmt_t *decl, symtable_t *globals)
{
    if (STMT_VAR_DECL(decl).member_count) {
        size_t total = layout_struct_members((struct_member_t *)STMT_VAR_DECL(decl).members,
                                             STMT_VAR_DECL(decl).member_count);
        STMT_VAR_DECL(decl).elem_size = total;
    } else if (STMT_VAR_DECL(decl).tag) {
        symbol_t *stype = symtable_lookup_struct(globals, STMT_VAR_DECL(decl).tag);
        if (!stype) {
            error_set(decl->line, decl->column, error_current_file,
                      error_current_function);
            return 0;
        }
        STMT_VAR_DECL(decl).elem_size = stype->struct_total_size;
    }
    return 1;
}

/* Copy union member metadata from a declaration to a symbol */
int copy_union_metadata(symbol_t *sym, union_member_t *members,
                        size_t count, size_t total)
{
    sym->total_size = total;
    if (!count)
        return 1;
    sym->members = malloc(count * sizeof(*sym->members));
    if (!sym->members)
        return 0;
    sym->member_count = count;
    for (size_t i = 0; i < count; i++) {
        union_member_t *m = &members[i];
        sym->members[i].name = vc_strdup(m->name);
        sym->members[i].type = m->type;
        sym->members[i].elem_size = m->elem_size;
        sym->members[i].offset = m->offset;
        sym->members[i].bit_width = m->bit_width;
        sym->members[i].bit_offset = m->bit_offset;
        sym->members[i].is_flexible = m->is_flexible;
    }
    return 1;
}

/* Copy struct member metadata from a declaration to a symbol */
int copy_struct_metadata(symbol_t *sym, struct_member_t *members,
                         size_t count, size_t total)
{
    sym->struct_total_size = total;
    if (!count)
        return 1;
    sym->struct_members = malloc(count * sizeof(*sym->struct_members));
    if (!sym->struct_members)
        return 0;
    sym->struct_member_count = count;
    for (size_t i = 0; i < count; i++) {
        struct_member_t *m = &members[i];
        sym->struct_members[i].name = vc_strdup(m->name);
        sym->struct_members[i].type = m->type;
        sym->struct_members[i].elem_size = m->elem_size;
        sym->struct_members[i].offset = m->offset;
        sym->struct_members[i].bit_width = m->bit_width;
        sym->struct_members[i].bit_offset = m->bit_offset;
        sym->struct_members[i].is_flexible = m->is_flexible;
    }
    return 1;
}

/* Copy aggregate member metadata from a declaration to a symbol */
int copy_aggregate_metadata(stmt_t *decl, symbol_t *sym,
                            symtable_t *globals)
{
    if (STMT_VAR_DECL(decl).type == TYPE_UNION)
        return copy_union_metadata(sym, STMT_VAR_DECL(decl).members,
                                   STMT_VAR_DECL(decl).member_count,
                                   STMT_VAR_DECL(decl).elem_size);

    if (STMT_VAR_DECL(decl).type == TYPE_STRUCT) {
        if (STMT_VAR_DECL(decl).member_count == 0 && STMT_VAR_DECL(decl).tag) {
            symbol_t *stype = symtable_lookup_struct(globals, STMT_VAR_DECL(decl).tag);
            if (!stype)
                return 0;
            return copy_struct_metadata(sym, stype->struct_members,
                                        stype->struct_member_count,
                                        stype->struct_total_size);
        }
        return copy_struct_metadata(sym,
                                    (struct_member_t *)STMT_VAR_DECL(decl).members,
                                    STMT_VAR_DECL(decl).member_count,
                                    STMT_VAR_DECL(decl).elem_size);
    }

    return 1;
}

