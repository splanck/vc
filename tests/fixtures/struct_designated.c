int main() {
    struct { int a; int b; } s = {.b = 5, .a = 3};
    return s.a + s.b;
}
