int foo() {
    char *p;
    p = __func__;
    return *(p + 1);
}
