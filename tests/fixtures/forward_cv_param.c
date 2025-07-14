int vcadd(const volatile int);

int main() {
    return vcadd(4);
}

int vcadd(const volatile int x) {
    return x + 2;
}
