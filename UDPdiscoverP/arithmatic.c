#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arithmatic.h"

 

void connect()
{
    printf("Connected to C extension...\n");
}

 
//return random value in range of 0-50
int randNum()
{
    int nRand = rand() % 50; 
    return nRand;
}
 
//add two number and return value
int addNum(int a, int b)
{
    int nAdd = a + b;
    return nAdd;
}

const char* giveChar()
{
//  static char * a[] = {"['1','2']", "['3','4']"};
  static char * a[] = {"ABC"};
  //strcpy(a,"['A','B','C']");

  return a[0];
}
