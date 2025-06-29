#include <stdint.h>
#include <stdio.h>

const char *get_hello_world_string() { return "Hello, World!"; }

int main(int argc, char **argv) {
  // Print the string returned by the function
  const char *hello_string = get_hello_world_string();
  printf("%s\n", hello_string);

  // Return 0 to indicate success
  return 0;
}