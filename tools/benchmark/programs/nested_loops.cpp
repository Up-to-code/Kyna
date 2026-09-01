#include <cstdio>

static int nestedWork(int n) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            total += i * j;
        }
    }
    return total;
}

int main() {
    std::printf("%d\n", nestedWork(200));
    return 0;
}
