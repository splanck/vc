union { int i; char c; } u;
int main() {
    u.i = 1;
    return u.c;
}
