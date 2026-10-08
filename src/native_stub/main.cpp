// The "native" env exists for unit tests (pio test -e native). This stub only
// makes `pio run -e native` succeed; the tests live in test/.
#include <stdio.h>

int main() {
  puts("Run the unit tests with: pio test -e native");
  return 0;
}
