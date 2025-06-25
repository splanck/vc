int add(int x) { return x + 1; }
int (*fn)(int) = add;
int main() { return fn(2); }
