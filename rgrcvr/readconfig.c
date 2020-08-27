#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <libconfig.h>

const char *configPath;
// the configuration file
static config_t cfg;
static config_setting_t *setting;

///////////////////// open config file ///////////////
int openConfigFile()
{
 // printf("test - config init\n");
  config_init(&cfg);

  /* Read the file. If there is an error, report it and exit. */

// The only thing we use this config file for is to get the path to the
// python config file. Seems like a kludge, but allows flexibility in
// system directory structure.
 // printf("test - read config file\n");
  if(! config_read_file(&cfg, "/home/odroid/projects/TangerineSDR-notes/mainctl/main.cfg"))
  {
    fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg),
            config_error_line(&cfg), config_error_text(&cfg));
    puts("ERROR - there is a problem with main.cfg configuration file");
    config_destroy(&cfg);
    return(EXIT_FAILURE);
  }
 // printf("test - look up config path\n");
  
  if(config_lookup_string(&cfg, "config_path", &configPath))
    {
    // printf("Setting config file path to: %s\n\n", configPath);
    }
  else
    fprintf(stderr, "No 'config_path' setting in configuration file main.cfg.\n");
    return(EXIT_FAILURE);
 // printf("test - config path=%s\n",configPath);
  return(0);
}

///// function to read config items from the (python) config file /////////
int rconfig(char * arg, char * result, int testThis) {
const char delimiters[] = " =";


  int rc = openConfigFile();
 // printf("retcode from config file open=%i\n",rc);

//printf("start fcn looking for %s\n", arg);
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
  return(1);
   }
  }
  free(cp);
  return(0);
}
