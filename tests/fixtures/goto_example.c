int main() {
    int i = 0;
start:
    if (i == 3)
        goto end;
    i = i + 1;
    goto start;
end:
    return i;
}
