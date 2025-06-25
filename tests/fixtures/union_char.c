union { int i; char c; } u;
int main() {
    u.c = 'A';
    return u.c;
}
