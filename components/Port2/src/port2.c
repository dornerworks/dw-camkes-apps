#include <camkes.h>
#include <stdio.h>
#include <string.h>

int run(void) {
  char *world = "world";

  /* Wait for Ping to message us. We can assume s1_data is
   * zeroed on startup by seL4.
   */
  while (!*(volatile char*)s1);
  printf("port2: received %s\n", (volatile char*)s1);

  printf("port2: sending %s...\n", world);
  strcpy((void*)s2, world);

  return 0;
}