#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <sys/mman.h>
#include <complex.h>
// #include <libconfig.h>


int main(int argc, char *argv[])
{
  FILE *fp;
  char name[64];
  double dialfreq;
  printf("starting\n");

  strcpy(name,"ft8_0_7075500_1_191106_2236.c2");
    if((fp = fopen(name, "r")) == NULL)
    {
      fprintf(stderr, "Cannot open output file %s.\n", name);
      return EXIT_FAILURE;
    }
    fread(&dialfreq, 1, 8, fp);
    printf("%f\n",dialfreq);
    

union {
    double dbl;
    uint8_t i8[sizeof(double) + 1];
} buf;

float complex myval;

printf("sizeof(dbl) = %ld\n", sizeof(double));
memset(buf.i8, 0x00, sizeof(buf.i8));
buf.dbl = dialfreq;
printf("%f\n", buf.dbl);
for(int i = 0; i < (int) sizeof(buf.i8) - 1; ++i) {
    printf("%02X", buf.i8[i]);
}
printf("\n");

for(int j=0; j <100000 ; j++)
 {
 fread(&myval, 1, sizeof(myval), fp);
 printf("%f   %f   \n",creal(myval), cimag(myval));
 sleep(0.1);
 }

    fclose(fp);


}
