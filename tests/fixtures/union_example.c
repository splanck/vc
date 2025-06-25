union { int a; char b; } u;

int main() {
    u.a = 65;
    return u.b;
}
