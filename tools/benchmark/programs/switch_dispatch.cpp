// Same no-fallthrough switch workload as the Kyna companion.
#include <iostream>
int main() {
  long long total = 0;
  for (int i = 0; i < 10000; ++i) {
    switch (i % 4) {
    case 0: total += 1; break;
    case 1: total += 2; break;
    case 2: total += 3; break;
    default: total += 4; break;
    }
  }
  std::cout << total << '\n';
}
