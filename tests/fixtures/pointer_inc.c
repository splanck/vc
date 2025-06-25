int main() {
    int a[3] = {1, 2, 3};
    int *p = a;
    ++p;
    p++;
    return *p;
}
