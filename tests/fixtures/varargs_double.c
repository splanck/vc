#define va_start(ap, last) (ap = &last + 1)
#define va_arg(ap, type) (*ap++)
#define va_end(ap) (ap = ap)

double sumd(int n, ...) {
    double *ap;
    va_start(ap, n);
    double t = 0.0;
    for (int i = 0; i < n; i++)
        t = t + va_arg(ap, double);
    va_end(ap);
    return t;
}
int main() {
    double r = sumd(2, 1.0, 2.0);
    return 0;
}
