#include <camkes.h>
#include <stdio.h>

static void handler(void) {
  static int fired = 0;
  printf("Callback fired!\n");
  if (!fired) {
    fired = 1;
    consume1_reg_callback(&handler, NULL);
  }
}

int run(void) {
  printf("Registering callback...\n");
  consume1_reg_callback(&handler, NULL);
  
  printf("Polling...\n");
  if (consume1_poll()) {
    printf("We found an event!\n");
  } else {
    printf("We didn't find an event\n");
  }

  printf("Waiting...\n");
  consume1_wait();
  printf("Unblocked by an event!\n");

  return 0;
}