#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

extern char rconfig(char * arg, char * result, int testThis);

void main(void){

char pathToRAMdisk[100];
char configresult[100];
int num_items = 0;
printf("start\n");
  num_items = rconfig("ramdisk_path",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - RAMdisk path setting not found in config.ini\n");
    }
  else
    {
    printf("RAMdisk path CONFIG RESULT = '%s'\n",configresult);
    strcpy(pathToRAMdisk,configresult);
    } 

printf("Ramdisk path =%s\n",pathToRAMdisk);

}
