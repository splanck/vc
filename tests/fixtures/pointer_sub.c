int main() {
    int a[2];
    int *p = a + 1;
    p = p - 1;
    a[0] = 7;
    return *p;
}
