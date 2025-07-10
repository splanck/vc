CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c99
OPTFLAGS ?=
MULTIARCH := $(shell $(CC) -print-multiarch 2>/dev/null || echo x86_64-linux-gnu)
GCC_INCLUDE_DIR := $(shell $(CC) -print-file-name=include)
BIN = vc
# The resulting binary accepts -c/--compile to assemble objects using cc
# Core compiler sources
CORE_SRC = src/main.c src/compile.c src/compile_stage.c src/compile_link.c src/compile_tokenize.c src/compile_parse.c src/compile_output.c src/startup.c src/command.c src/cli.c src/cli_env.c src/cli_opts.c src/lexer.c src/lexer_ident.c src/lexer_scan_numeric.c src/ast_expr.c src/ast_stmt_create.c src/ast_stmt_free.c src/ast_clone.c src/parser_core.c src/parser_toplevel.c src/parser_toplevel_func.c src/parser_toplevel_var.c src/symtable_core.c src/symtable_globals.c src/symtable_struct.c src/parser_expr.c src/parser_expr_primary.c src/parser_expr_binary.c src/parser_expr_ops.c src/parser_expr_literal.c src/parser_init.c \
           src/parser_decl_var.c src/parser_decl_struct.c src/parser_decl_enum.c \
           src/parser_flow.c src/parser_stmt.c src/parser_types.c \
           src/semantic_expr.c src/semantic_expr_const.c src/semantic_expr_cast.c \
           src/semantic_arith.c src/semantic_mem.c src/semantic_call.c \
           src/semantic_loops.c src/semantic_control.c src/semantic_init.c src/semantic_var.c src/semantic_stmt.c \
           src/semantic_block.c src/semantic_decl.c src/semantic_decl_stmt.c src/semantic_expr_stmt.c src/semantic_label.c src/semantic_return.c src/semantic_static_assert.c \
           src/semantic_layout.c src/semantic_inline.c src/semantic_global.c src/consteval.c src/error.c src/ir_core.c src/ir_const.c src/ir_memory.c src/ir_control.c src/ir_global.c \
           src/codegen.c src/codegen_mem.c src/codegen_load.c src/codegen_store.c src/codegen_arith_int.c src/codegen_arith_float.c src/codegen_branch.c \
           src/codegen_float.c src/codegen_complex.c src/codegen_x86.c \
           src/regalloc.c src/regalloc_x86.c src/strbuf.c src/util.c src/vector.c src/ir_dump.c src/ir_builder.c src/ast_dump.c src/label.c \
           src/preproc_expand.c src/preproc_builtin.c src/preproc_args.c src/preproc_table.c src/preproc_expr.c src/preproc_cond.c src/preproc_file.c \
           src/preproc_directives.c src/preproc_file_io.c src/preproc_include.c src/preproc_includes.c src/preproc_path.c \
           src/token_names.c

# Optional optimization sources
OPT_SRC = src/opt.c src/opt_constprop.c src/opt_cse.c src/opt_fold.c src/opt_licm.c src/opt_dce.c src/opt_inline.c src/opt_inline_helpers.c src/opt_unreachable.c src/opt_alias.c
# Additional sources can be specified by the user
EXTRA_SRC ?=
# Final source list
SRC = $(CORE_SRC) $(OPT_SRC) $(EXTRA_SRC)
OBJ := $(SRC:.c=.o)
HDR = include/token.h include/token_names.h include/ast.h include/ast_clone.h include/ast_expr.h include/ast_stmt.h include/parser.h include/symtable.h include/semantic.h     include/consteval.h include/semantic_expr.h include/semantic_arith.h include/semantic_mem.h include/semantic_call.h include/semantic_loops.h include/semantic_control.h include/semantic_stmt.h include/semantic_decl_stmt.h include/semantic_inline.h include/semantic_var.h include/semantic_layout.h include/semantic_init.h include/semantic_global.h \
    include/ir_core.h include/ir_const.h include/ir_memory.h include/ir_control.h include/ir_builder.h include/ir_global.h include/ir_dump.h include/ast_dump.h include/opt.h include/codegen.h include/codegen_mem.h include/codegen_loadstore.h include/codegen_arith.h include/codegen_arith_int.h include/codegen_arith_float.h include/codegen_branch.h include/strbuf.h \
    include/util.h include/command.h include/cli.h include/vector.h include/regalloc_x86.h include/label.h include/error.h include/lexer_internal.h \
    include/opt_inline_helpers.h \
    include/preproc.h include/preproc_file.h include/preproc_macros.h include/preproc_includes.h include/preproc_expr.h include/preproc_cond.h include/preproc_path.h include/parser_types.h include/parser_core.h include/startup.h include/compile_stage.h
PREFIX ?= /usr/local
INCLUDEDIR ?= $(PREFIX)/include/vc
MANDIR ?= $(PREFIX)/share/man

all: $(BIN)

test: $(BIN)
	./tests/run_tests.sh

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(OPTFLAGS) -o $@ $(OBJ)

install: $(BIN)
	install -d $(DESTDIR)$(INCLUDEDIR)
	install -m 644 $(HDR) $(DESTDIR)$(INCLUDEDIR)
	install -d $(DESTDIR)$(PREFIX)/bin
	install $(BIN) $(DESTDIR)$(PREFIX)/bin/
	install -d $(DESTDIR)$(MANDIR)/man1
	install -m 644 man/vc.1 $(DESTDIR)$(MANDIR)/man1/

clean:
	rm -f $(BIN) $(OBJ)

.PHONY: all clean install test
src/main.o: src/main.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/main.c -o src/main.o
src/compile.o: src/compile.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/compile.c -o src/compile.o
src/compile_stage.o: src/compile_stage.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/compile_stage.c -o src/compile_stage.o
src/compile_link.o: src/compile_link.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/compile_link.c -o src/compile_link.o
src/compile_tokenize.o: src/compile_tokenize.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/compile_tokenize.c -o src/compile_tokenize.o
src/compile_parse.o: src/compile_parse.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/compile_parse.c -o src/compile_parse.o
src/compile_output.o: src/compile_output.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/compile_output.c -o src/compile_output.o
src/command.o: src/command.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/command.c -o src/command.o

src/startup.o: src/startup.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/startup.c -o src/startup.o


src/cli.o: src/cli.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/cli.c -o src/cli.o
src/cli_env.o: src/cli_env.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/cli_env.c -o src/cli_env.o
src/cli_opts.o: src/cli_opts.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/cli_opts.c -o src/cli_opts.o

src/lexer.o: src/lexer.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/lexer.c -o src/lexer.o

src/lexer_ident.o: src/lexer_ident.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/lexer_ident.c -o src/lexer_ident.o

src/lexer_scan_numeric.o: src/lexer_scan_numeric.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/lexer_scan_numeric.c -o src/lexer_scan_numeric.o

src/ast_expr.o: src/ast_expr.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/ast_expr.c -o src/ast_expr.o
src/ast_stmt_create.o: src/ast_stmt_create.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/ast_stmt_create.c -o src/ast_stmt_create.o

src/ast_stmt_free.o: src/ast_stmt_free.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/ast_stmt_free.c -o src/ast_stmt_free.o



src/ast_clone.o: src/ast_clone.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/ast_clone.c -o src/ast_clone.o

src/parser_core.o: src/parser_core.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/parser_core.c -o src/parser_core.o

src/parser_toplevel.o: src/parser_toplevel.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/parser_toplevel.c -o src/parser_toplevel.o
src/parser_toplevel_func.o: src/parser_toplevel_func.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/parser_toplevel_func.c -o src/parser_toplevel_func.o

src/parser_toplevel_var.o: src/parser_toplevel_var.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/parser_toplevel_var.c -o src/parser_toplevel_var.o

src/symtable_core.o: src/symtable_core.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/symtable_core.c -o src/symtable_core.o

src/symtable_globals.o: src/symtable_globals.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/symtable_globals.c -o src/symtable_globals.o

src/symtable_struct.o: src/symtable_struct.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/symtable_struct.c -o src/symtable_struct.o



src/parser_expr.o: src/parser_expr.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/parser_expr.c -o src/parser_expr.o

src/parser_expr_primary.o: src/parser_expr_primary.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/parser_expr_primary.c -o src/parser_expr_primary.o

src/parser_expr_binary.o: src/parser_expr_binary.c $(HDR)
		$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/parser_expr_binary.c -o src/parser_expr_binary.o

src/parser_expr_ops.o: src/parser_expr_ops.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/parser_expr_ops.c -o src/parser_expr_ops.o

src/parser_expr_literal.o: src/parser_expr_literal.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/parser_expr_literal.c -o src/parser_expr_literal.o

src/parser_init.o: src/parser_init.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/parser_init.c -o src/parser_init.o

src/parser_decl_var.o: src/parser_decl_var.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/parser_decl_var.c -o src/parser_decl_var.o
	
src/parser_decl_struct.o: src/parser_decl_struct.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/parser_decl_struct.c -o src/parser_decl_struct.o
	
src/parser_decl_enum.o: src/parser_decl_enum.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/parser_decl_enum.c -o src/parser_decl_enum.o
	
src/parser_flow.o: src/parser_flow.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/parser_flow.c -o src/parser_flow.o

src/parser_stmt.o: src/parser_stmt.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/parser_stmt.c -o src/parser_stmt.o

src/parser_types.o: src/parser_types.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/parser_types.c -o src/parser_types.o

src/semantic_expr.o: src/semantic_expr.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/semantic_expr.c -o src/semantic_expr.o

src/semantic_expr_const.o: src/semantic_expr_const.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/semantic_expr_const.c -o src/semantic_expr_const.o
	
src/semantic_expr_cast.o: src/semantic_expr_cast.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/semantic_expr_cast.c -o src/semantic_expr_cast.o

src/semantic_arith.o: src/semantic_arith.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/semantic_arith.c -o src/semantic_arith.o

src/semantic_mem.o: src/semantic_mem.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/semantic_mem.c -o src/semantic_mem.o

src/semantic_call.o: src/semantic_call.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/semantic_call.c -o src/semantic_call.o

src/semantic_loops.o: src/semantic_loops.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/semantic_loops.c -o src/semantic_loops.o

src/semantic_control.o: src/semantic_control.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/semantic_control.c -o src/semantic_control.o
src/semantic_init.o: src/semantic_init.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/semantic_init.c -o src/semantic_init.o

src/semantic_var.o: src/semantic_var.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/semantic_var.c -o src/semantic_var.o


src/semantic_stmt.o: src/semantic_stmt.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/semantic_stmt.c -o src/semantic_stmt.o
src/semantic_block.o: src/semantic_block.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/semantic_block.c -o src/semantic_block.o
src/semantic_decl.o: src/semantic_decl.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/semantic_decl.c -o src/semantic_decl.o
src/semantic_decl_stmt.o: src/semantic_decl_stmt.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/semantic_decl_stmt.c -o src/semantic_decl_stmt.o
src/semantic_expr_stmt.o: src/semantic_expr_stmt.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/semantic_expr_stmt.c -o src/semantic_expr_stmt.o
src/semantic_label.o: src/semantic_label.c $(HDR)
		$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/semantic_label.c -o src/semantic_label.o
src/semantic_return.o: src/semantic_return.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/semantic_return.c -o src/semantic_return.o
src/semantic_static_assert.o: src/semantic_static_assert.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/semantic_static_assert.c -o src/semantic_static_assert.o
src/semantic_layout.o: src/semantic_layout.c $(HDR)
	        $(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/semantic_layout.c -o src/semantic_layout.o
src/semantic_inline.o: src/semantic_inline.c $(HDR)
		$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/semantic_inline.c -o src/semantic_inline.o


src/semantic_global.o: src/semantic_global.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/semantic_global.c -o src/semantic_global.o

src/consteval.o: src/consteval.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/consteval.c -o src/consteval.o

src/error.o: src/error.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/error.c -o src/error.o

src/ir_core.o: src/ir_core.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/ir_core.c -o src/ir_core.o

src/ir_const.o: src/ir_const.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/ir_const.c -o src/ir_const.o

src/ir_memory.o: src/ir_memory.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/ir_memory.c -o src/ir_memory.o

src/ir_control.o: src/ir_control.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/ir_control.c -o src/ir_control.o

src/ir_builder.o: src/ir_builder.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/ir_builder.c -o src/ir_builder.o

src/ir_global.o: src/ir_global.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/ir_global.c -o src/ir_global.o

src/codegen.o: src/codegen.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/codegen.c -o src/codegen.o

src/codegen_mem.o: src/codegen_mem.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/codegen_mem.c -o src/codegen_mem.o

src/codegen_load.o: src/codegen_load.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/codegen_load.c -o src/codegen_load.o

src/codegen_store.o: src/codegen_store.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/codegen_store.c -o src/codegen_store.o

src/codegen_arith.o: src/codegen_arith.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/codegen_arith.c -o src/codegen_arith.o
src/codegen_float.o: src/codegen_float.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/codegen_float.c -o src/codegen_float.o
src/codegen_arith_int.o: src/codegen_arith_int.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/codegen_arith_int.c -o src/codegen_arith_int.o
src/codegen_arith_float.o: src/codegen_arith_float.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/codegen_arith_float.c -o src/codegen_arith_float.o

src/codegen_complex.o: src/codegen_complex.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/codegen_complex.c -o src/codegen_complex.o

src/codegen_x86.o: src/codegen_x86.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/codegen_x86.c -o src/codegen_x86.o


src/codegen_branch.o: src/codegen_branch.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/codegen_branch.c -o src/codegen_branch.o

src/regalloc.o: src/regalloc.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/regalloc.c -o src/regalloc.o

src/regalloc_x86.o: src/regalloc_x86.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/regalloc_x86.c -o src/regalloc_x86.o

src/strbuf.o: src/strbuf.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/strbuf.c -o src/strbuf.o

src/util.o: src/util.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/util.c -o src/util.o

src/vector.o: src/vector.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/vector.c -o src/vector.o

src/ir_dump.o: src/ir_dump.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/ir_dump.c -o src/ir_dump.o

src/ast_dump.o: src/ast_dump.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/ast_dump.c -o src/ast_dump.o

src/label.o: src/label.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/label.c -o src/label.o

src/token_names.o: src/token_names.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/token_names.c -o src/token_names.o

src/preproc_expand.o: src/preproc_expand.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/preproc_expand.c -o src/preproc_expand.o
src/preproc_builtin.o: src/preproc_builtin.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/preproc_builtin.c -o src/preproc_builtin.o

src/preproc_args.o: src/preproc_args.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/preproc_args.c -o src/preproc_args.o


src/preproc_table.o: src/preproc_table.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/preproc_table.c -o src/preproc_table.o

src/preproc_expr.o: src/preproc_expr.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/preproc_expr.c -o src/preproc_expr.o
src/preproc_cond.o: src/preproc_cond.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/preproc_cond.c -o src/preproc_cond.o


src/preproc_file.o: src/preproc_file.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/preproc_file.c -o src/preproc_file.o

src/preproc_directives.o: src/preproc_directives.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/preproc_directives.c -o src/preproc_directives.o

src/preproc_file_io.o: src/preproc_file_io.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/preproc_file_io.c -o src/preproc_file_io.o

src/preproc_include.o: src/preproc_include.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/preproc_include.c -o src/preproc_include.o
src/preproc_includes.o: src/preproc_includes.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/preproc_includes.c -o src/preproc_includes.o
src/preproc_path.o: src/preproc_path.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) \
		-DMULTIARCH=\"$(MULTIARCH)\" \
		-DGCC_INCLUDE_DIR=\"$(GCC_INCLUDE_DIR)\" \
		-Iinclude -c src/preproc_path.c -o src/preproc_path.o

src/opt.o: src/opt.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/opt.c -o src/opt.o

src/opt_constprop.o: src/opt_constprop.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/opt_constprop.c -o src/opt_constprop.o

src/opt_fold.o: src/opt_fold.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/opt_fold.c -o src/opt_fold.o

src/opt_licm.o: src/opt_licm.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/opt_licm.c -o src/opt_licm.o

src/opt_cse.o: src/opt_cse.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/opt_cse.c -o src/opt_cse.o

src/opt_dce.o: src/opt_dce.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/opt_dce.c -o src/opt_dce.o

src/opt_inline.o: src/opt_inline.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/opt_inline.c -o src/opt_inline.o

src/opt_inline_helpers.o: src/opt_inline_helpers.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/opt_inline_helpers.c -o src/opt_inline_helpers.o


src/opt_unreachable.o: src/opt_unreachable.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/opt_unreachable.c -o src/opt_unreachable.o

src/opt_alias.o: src/opt_alias.c $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -c src/opt_alias.c -o src/opt_alias.o
