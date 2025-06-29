#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Example functions that do some arithmetic that we want to obfuscate.
static int can_i_add(int a, int b) { return a + b; }

static int can_i_substract(int a, int b) { return a - b; }

static int can_i_or(int a, int b) {
  // This is a simple bitwise OR operation.
  return a | b;
}

static int can_i_and(int a, int b) {
  // This is a simple bitwise AND operation.
  return a & b;
}

static int can_i_xor(int a, int b) {
  // This is a simple bitwise XOR operation.
  return a ^ b;
}

static int getSecretValue() {
  // Do some random arithmetic operation on a value seeded from the current
  // time to make it less obvious.
  int value = arc4random();
  // Do some additions and subtractions to further obfuscate.
  value = can_i_xor(value, 0xfeac);
  value = can_i_add(value, 10);
  value = can_i_and(value, 15);
  value = can_i_add(value, 3);
  value = can_i_or(value, 7);
  value = can_i_substract(value, 5);
  value = can_i_substract(value, 1);
  return value;
}

int main(int argc, char **argv) {

  assert(can_i_add(1, 1) == 2 && "add(1, 1) should be 2");
  assert(can_i_substract(5, 3) == 2 && "subtract(5, 3) should be 2");
  assert(can_i_add(57, 43) == 100 && "add(57, 43) should be 100");
  assert(can_i_substract(100, 50) == 50 && "subtract(100, 50) should be 50");
  assert(can_i_or(42, 0xffff) == 0xffff && "or(42, 0xffff) should be 0xffff");
  assert(can_i_and(0x0f0f, 0xf0f0) == 0x0000 &&
         "and(0x0f0f, 0xf0f0) should be 0x0000");
  assert(can_i_xor(0x1234, 0x5678) == 0x444c &&
         "xor(0x1234, 0x5678) should be 0x444c");

  // Print the obfuscated secret value.
  int secretValue = getSecretValue();
  printf("Obfuscated secret value: %d\n", secretValue);

  // Return 0 to indicate success
  return 0;
}
