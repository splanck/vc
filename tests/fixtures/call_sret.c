struct S { int a; };
struct S foo(void) { struct S s = { 5 }; return s; }
int main() { struct S s = foo(); return s.a; }
