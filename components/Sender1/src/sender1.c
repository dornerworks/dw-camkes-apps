#include <camkes.h>
#include <stdio.h>
int run(void) {
  char buf[50] = {0};
  printf("Buffer available at %p, contains %s\n", (void *)buf, buf);
  int *a = (int *) 0xaaaaaa;
  *a = 0x12345678;
  printf("Test\n");
  printf("Buffer now contains %s\n", buf);
  const char *s = "Message from sender 1";
  out1_print(s);
  return 0;
}
