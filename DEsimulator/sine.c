#include <stdio.h>
#include <stdlib.h>
#include <math.h>

struct dataSample
	{
	float I_val;
	float Q_val;
	};

struct dataBuf
	{
	double timeStamp;
	struct dataSample myDataSample[1024];
	};

void main() {
float I;
float Q;
float A;
A = 1.0;



struct dataBuf myBuffer;
struct dataSample mySample;



for (int i = 0; i < 1024; i++) {
	I = A * (sin ( (float)i / 57.295778666));
	Q = A * (cos ( (float)i / 57.295778666));
	//mySample.I_val = I;
	//mySample.Q_val = Q;
	myBuffer.timeStamp = (float) i;
	myBuffer.myDataSample[i].I_val = I;
	myBuffer.myDataSample[i].Q_val = Q;

	printf("%d   %f       %f \n",i, I, Q);
  }
}
	
