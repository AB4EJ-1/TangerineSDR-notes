#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include <string.h>
#include <libconfig.h>

#include <dirent.h>
#include <errno.h>

#include <unistd.h>
#include <stddef.h>

int rconfig(char * arg, char * result) {
const char delimiters[] = " =";
printf("start fcn looking for %s\n", arg);

FILE *fp;
char *line = NULL;
size_t len = 0;
ssize_t read;
char *token, *cp;

fp = fopen("/home/odroid/projects/TangerineSDR-notes/flask/config.ini", "r");
if (fp == NULL)
  {
  puts("could not open");
  exit(-1);
  }

puts("read");
while ((read = getline(&line, &len, fp)) != -1) {
  printf("line length %zu: ",read);
  printf("%s \n",line);

  cp = strdup(line);  // allocate enuff memory for a copy of this
  printf("cp=%s\n",cp);
  token = strtok(cp, delimiters);
  printf("first token='%s'\n",token);

  if(strcmp(arg,token) == 0)
   {
  token = strtok(NULL, delimiters);
  printf("second token=%s\n",token);
  strcpy(result,token);
  free(cp);
  return(1);
   }

  }
  free(cp);
  return(0);
}
void main(int argc, char* argv[]) {
 char result[100];
 int num_items = 0;
 puts("start");
 printf("looking for '%s'\n",argv[1]);
 num_items = rconfig(argv[1],result);
 printf("RESULT = '%s'\n",result);
 printf("len =%lu\n",strlen(result));

 }


