void callee(void);

void caller(int n)
{
    char buf[n];
    callee();
    buf[0] = 0;
}
