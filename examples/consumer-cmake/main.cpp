#include <iostream>

int main() {
  std::cout << "WebIQ Modbus IOHandler consumer example (link test)" << std::endl;
  // This example links against WebIQ::ioh_modbus but does not call into the
  // library, keeping runtime setup simple. It verifies that find_package and
  // target linking work end-to-end.
  return 0;
}

