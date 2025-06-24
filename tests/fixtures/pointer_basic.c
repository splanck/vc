int main() {
    int x;
    int *p;
    p = &x;
    x = 42;
    return *p;
}
