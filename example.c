
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
   printf("Example Program Running...\n");
   int i = 0;
   for (i = 0; i < 200000; i++) {
      i += 1;
   }

   for (i = 0; i < 120; i++) {
      sleep(1);
   }

   return 0;
}
