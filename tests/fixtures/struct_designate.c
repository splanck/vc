struct Point { int x; int y; };
int main() {
    struct Point p = { .y = 5, .x = 1 };
    return p.y - p.x;
}
