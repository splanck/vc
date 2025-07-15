# Memory Helpers

Some parts of the compiler store heap allocated strings, `macro_t` objects or
AST nodes inside `vector_t` containers. The functions `free_string_vector`,
`free_macro_vector`, `free_func_list_vector` and `free_glob_list_vector` release
these structures safely.

`free_string_vector` walks a vector of `char *`, frees each string and then
calls `vector_free` on the container. Use it whenever a vector holds
strings duplicated with `malloc` or `vc_strdup`.

`free_macro_vector` performs the same task for vectors of `macro_t`. It calls
`macro_free` on each element before freeing the vector itself.

`free_func_list_vector` frees vectors of `func_t *`. Each stored function is
released with `ast_free_func` before the vector itself is freed.

`free_glob_list_vector` does the same for vectors of `stmt_t *`, calling
`ast_free_stmt` on every element.

```c
vector_t paths;
vector_init(&paths, sizeof(char *));
vector_push(&paths, vc_strdup("include"));
...
free_string_vector(&paths); /* releases strings and vector */
```

The helpers are declared in [util.h](../include/util.h) and implemented in
[util.c](../src/util.c).
