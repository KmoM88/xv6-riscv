#include "kernel/types.h"
#include "user/user.h"

void
func_c(void)
{
  // This triggers sys_pause in the kernel, printing our backtrace!
  pause(1);
}

void
func_b(void)
{
  func_c();
}

void
func_a(void)
{
  func_b();
}

int
main(int argc, char *argv[])
{
  printf("Starting backtrace test utility...\n");
  func_a();
  exit(0);
}
