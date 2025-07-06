# Memory Helpers

Some parts of the compiler store heap allocated strings or `macro_t` objects
inside `vector_t` containers. The functions `free_string_vector` and
`free_macro_vector` release these structures safely.

`free_string_vector` walks a vector of `char *`, frees each string and then
calls `vector_free` on the container. Use it whenever a vector holds
strings duplicated with `malloc` or `vc_strdup`.

`free_macro_vector` performs the same task for vectors of `macro_t`. It calls
`macro_free` on each element before freeing the vector itself.

```c
vector_t paths;
vector_init(&paths, sizeof(char *));
vector_push(&paths, vc_strdup("include"));
...
free_string_vector(&paths); /* releases strings and vector */
```

The helpers are declared in [util.h](../include/util.h) and implemented in
[util.c](../src/util.c).
