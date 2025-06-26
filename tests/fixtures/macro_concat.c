#define JOIN(a,b) a##b
int foobar = 5;
int main() { return JOIN(foo,bar); }
