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

*/

#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include <string.h>
#include <libconfig.h>
#include "digital_rf.h"
#include <dirent.h>
#include <errno.h>
#include "de_signals.h"
#include <unistd.h>
//#include <json.h>
#include <pthread.h>
#include <time.h>
#include <complex.h>
#include <math.h>
#include <time.h>
#include "CUnit/CUnit.h"
#include "CUnit/Basic.h"

static FT8record ft8list[100];
static int listpointer = 0;
static uint32_t startTime;


int main(FT8record ft8report){

  int sendIt = 0;
  printf("ft8record received in sendPSK, senderCall= %s\n",ft8report,senderCallsign);
  if(listpointer == 0)
   {
   startTime = (uint32_t)time(NULL);
   }
  else
   {
   uint32_t nowTime = (uint32_t)time(NULL);
   if(nowTime > (startTime + 399))  // has 5 minutes elapsed?
    sendIt = 1;
   }

  // put record into list
  memcpy(ft8list[listpointer], ft8report, sizeof(ft8report));  
  listpointer++;

// have we logged 100 calls or has it been 300 seconds since we started
  if(listpointer > 100 || sendIt ==1)
   {


   }

  return 0;
}




