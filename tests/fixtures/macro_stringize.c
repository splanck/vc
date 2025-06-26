#define TO_STR(x) #x
int main() {
    char *s;
    s = TO_STR(hello);
    return 0;
}
