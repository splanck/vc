int main() {
    int arr[2];
    int i = 1;
    int *p = arr;
    p = p + i;
    arr[1] = 4;
    return *p;
}
