#define va_start(ap, last) (ap = &last + 1)
#define va_arg(ap, type) (*ap++)
#define va_end(ap) (ap = ap)

int sum(int n, ...) {
    int *ap;
    va_start(ap, n);
    int t = 0;
    for (int i = 0; i < n; i++)
        t += va_arg(ap, int);
    va_end(ap);
    return t;
}
int main() { return sum(3, 1, 2, 3); }
