int main() {
    int a[3];
    int *p1 = a;
    int *p2 = a + 2;
    return p2 - p1;
}
