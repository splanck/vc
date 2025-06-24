int main() {
    int a[2];
    int *p = a;
    p = p + 1;
    a[1] = 5;
    return *p;
}
