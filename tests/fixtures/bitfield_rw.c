struct {
    unsigned a:3;
    unsigned b:5;
} s;

int main() {
    s.a = 5;
    s.b = 17;
    return s.a + s.b;
}
