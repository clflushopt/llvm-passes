#include <iostream>
#include <string>
#include <vector>

static int aVeryComplicatedFunctionWithManyBranches(int x) {
  switch (x) {
  case 1:
    return 10;
  case 2:
    return 20;
  case 3:
    return 30;
  default:
    return -1;
  }
}

static std::string aSlightlyMoreConvolutedFunction() {
  // Compute some random polynomial value.
  int x = 42;
  int y = 0;
  for (int i = 0; i < 10; ++i) {
    y += (x * x) - (i * 2) + (x % 3) - (i / 2);
  }

  if (y < 0) {
    // Do some random loop computation followed by a string return.
    std::vector<int> vec;
    for (int i = 0; i < 100; ++i) {
      vec.push_back(i * i);
    }

    std::string result = "Computed value: ";
    for (const auto &v : vec) {
      result += std::to_string(v) + " ";
    }
    return result;
  }

  return "Final result: " + std::to_string(y);
}

int main() {
  int result1 = aVeryComplicatedFunctionWithManyBranches(5);
  std::cout << "Result of complicated function: " << result1 << std::endl;

  std::string result2 = aSlightlyMoreConvolutedFunction();
  std::cout << "Result of convoluted function: " << result2 << std::endl;

  return 0;
}