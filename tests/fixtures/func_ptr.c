int add(int a, int b) { return a + b; }
int (*fp)(int,int);
int main() {
    fp = add;
    return fp(2,3);
}
