#include <camkes.h>

int run(void) {
  const char *s = "Message from sender 2";
  out2_print(s);
  return 0;
}