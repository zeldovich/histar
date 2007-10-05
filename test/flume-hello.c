#include <stdio.h>

int
main (int argc, char *argv[])
{
  setvbuf (stdout, 0, _IONBF, 0);
  printf ("Hello world!\n");

  return 0;
}
