#include <camkes.h>
#include <stdio.h>
#include <string.h>

int run(void) {
  char *hello = "hello";

  printf("port1: sending %s...\n", hello);
  strcpy((void*)d1, hello);

  /* Wait for Pong to reply. We can assume d2_data is
   * zeroed on startup by seL4.
   */
  while (!*(volatile char*)d2);
  printf("port1: received %s.\n", d2);

  return 0;
}