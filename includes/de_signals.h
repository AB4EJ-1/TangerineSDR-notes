/*
de_signals.h
Maps program mnemonics to 2-byte commands to be passed to DE
*/
#define STATUS_INQUIRY      "S?"
#define LED1_ON             "Y1"
#define LED1_OFF            "N1"
#define TIME_INQUIRY        "T?"
#define TIME_STAMP          "TS"
#define CREATE_CHANNEL      "CC"
#define CONFIG_CHANNELS     "CH"
#define UNDEFINE_CHANNEL    "UC"
#define FIREHOSE_SERVER     "FH"
#define START_DATA_COLL     "SC"
#define STOP_DATA_COLL      "XC"
#define DEFINE_FT8_CHAN     "FT"
#define START_FT8_COLL      "SF"
#define STOP_FT8_COLL       "XF"
#define LED_SET             "SB"  // in case we need to send a binary LED set byte
#define UNLINK              "UL"
#define HALT_DE             "XX"

// buffer for A/D data from DE
struct dataSample
	{
	float I_val;
	float Q_val;
	};
typedef struct dataBuf
	{
    char bufType[2];
	union {  // this space contains buffer length for data buffer, error code for NAK
	  long bufCount;
      char errorCode[2];
	  } dval;
	long timeStamp;
    union {
     int channelNo;
     int channelCount;
     };
    double centerFreq;
 
	//struct dataSample myDataSample[1024]; this is the logical layout using dataSample.
    //    Below is what Digital RF reequires to be able to understand the samples.
    //    In the array, starting at zero, sample[j] = I, sample[j+1] = Q (complex data)
    struct dataSample theDataSample[1024];  // should be double the number of samples
	} DATABUF ;

/*
typedef struct responseBuf
	{
	char buftype[2];
	char responseCode[2];
    } RESPONSEBUF;
*/

typedef struct configChannelRequest
	{
    char cmd[2];
	uint16_t configPort;  // Port C
	uint16_t dataPort;    // Port F
	} CONFIGBUF;

struct channelBlock
	{
	int channelNo;
    int antennaPort;
    double channelFreq;
    double channelBandwidth;
    };
typedef struct channelBuf
	{
    char chCommand[2];
    struct channelBlock channelDef[16];
    } CHANNELBUF;

