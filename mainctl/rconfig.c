/* Copyright (C) 2019 The University of Alabama
* Author: William (Bill) Engelke, AB4EJ
* With funding from the Center for Advanced Public Safety and 
* The National Science Foundation.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
* External packages:

*/

#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <libconfig.h>

const char *configPath;

////// function to read config items from the (python) config file /////////
int rconfig(char * arg, char * result, int testThis) {
const char delimiters[] = " =";
printf("start fcn looking for %s\n", arg);
FILE *fp;
char *line = NULL;
size_t len = 0;
ssize_t read;
char *token, *cp;
if (testThis)
  {
  fp = fopen( "/home/odroid/projects/TangerineSDR-notes/flask/config.ini", "r");
  }
else
  fp = fopen(configPath, "r");
if (fp == NULL)
  {
  printf("ERROR - could not open config file at %s\n",configPath);
  printf("ABEND 102");
  exit(-1);
  }
//puts("read config");
while ((read = getline(&line, &len, fp)) != -1) {
 // printf("line length %zu: ",read);
 // printf("%s \n",line);
  cp = strdup(line);  // allocate enuff memory for a copy of this
  //printf("cp=%s\n",cp);
  token = strtok(cp, delimiters);
 // printf("first token='%s'\n",token);
  if(strcmp(arg,token) == 0)
   {
  token = strtok(NULL, delimiters);
 // printf("second token=%s\n",token);
 // printf("config value found = '%s', length = %lu\n",token,strlen(token));
  strncpy(result,token,strlen(token)-1);
  result[strlen(token)-1] = 0x00;  // terminate the string
  free(cp);
  fclose(fp);
  return(1);
   }
  }
  free(cp);
  fclose(fp);
  return(0);
}
