int main() {
    int arr[3] = {1, 2, 3};
    int i = 2;
    int *p = arr + i;
    p = p - i;
    return *p;
}
