// Same typed indirect-call workload as the Kyna companion.
#include <iostream>
long long twice(long long value) { return value * 2; }
int main() {
  auto callback = &twice;
  long long total = 0;
  for (int i = 0; i < 10000; ++i) total += callback(i);
  std::cout << total << '\n';
}
