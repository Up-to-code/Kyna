#include <cstdio>

static bool isPrime(int n) {
    if (n < 2) return false;
    for (int d = 2; d * d <= n; ++d) {
        if (n % d == 0) return false;
    }
    return true;
}

static int countPrimes(int limit) {
    int count = 0;
    for (int i = 2; i <= limit; ++i) {
        if (isPrime(i)) ++count;
    }
    return count;
}

int main() {
    std::printf("%d\n", countPrimes(50000));
    return 0;
}
