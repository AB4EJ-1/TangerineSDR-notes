/*
de_signals.h
Maps program mnemonics to 2-byte commands to be passed to DE
*/
#define STATUS_INQUIRY      "S?"  // asks DE to send "OK" or "AK"
#define DATARATE_INQUIRY    "R?"  // asks DE to send a table of supported data rates
#define DATARATE_RESPONSE   "DR"  // response: start of data rate table
#define LED1_ON             "Y1"  // asks DE to turn on LED1
#define LED1_OFF            "N1"  // asks DE to turn off LED1
#define TIME_INQUIRY        "T?"  // asks DE to send the time from GPSDO
#define TIME_STAMP          "TS"  // response: time of day
#define CREATE_CHANNEL      "CC"  // asks DE to create set of data channels
#define CONFIG_CHANNELS     "CH"  // gives DE channel configurations
#define UNDEFINE_CHANNEL    "UC"  // asks DE to drop its set of data channels
#define FIREHOSE_SERVER     "FH"  // puts DE into firehose mode
#define START_DATA_COLL     "SC"  // asks DE to start collecting data in ringbuffer mode
#define STOP_DATA_COLL      "XC"  // asks DE to stop collecting data in ringbuffer mode
#define DEFINE_FT8_CHAN     "FT"  // gives DE configuration for one FT8 channel
#define START_FT8_COLL      "SF"  // asks DE to start collecting FT8 data on all FT8 channels
#define STOP_FT8_COLL       "XF"  // asks DE to stop collecting FT8 data
#define LED_SET             "SB"  // in case we need to send a binary LED set byte
#define UNLINK              "UL"  // asks DE to disconnect from this LH
#define HALT_DE             "XX"  // asks DE to halt
#define RESTART_DE          "XR"  // asks DE to do a cold start
#define FT_DATA_BUFFER      "FT"  // this is an FT8 data packet
#define RG_DATA_BUFFER      "RG"  // this is a ringbuffer (or firehose) data packet
#define STATUS_OK           "OK"  // or "AK"  // DE is alive / last command was accepted


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
    struct dataSample theDataSample[1024];  
	} DATABUF ;

typedef struct VITAdataBuf
 {
 char VITA_hdr1[2];  // rightmost 4 bits is a packet counter
 int16_t  VITA_packetsize;
 char stream_ID[4];
 uint32_t time_stamp;
 uint64_t sample_count;
 struct dataSample theDataSample[1024];
 } VITABUF;


struct datarateEntry
    {
    int rateNumber;
    int rateValue;
    };
// This is the buffer type sent from DE to LH when
// LH has requested a list of supported data rates.
typedef struct datarateBuf
	{
	char buftype[2];
	struct datarateEntry dataRate[20];
    } DATARATEBUF;

typedef struct comboBuf
    {
    union
     {
      DATARATEBUF dbuf;
      char dbufc[175];
     }  ;
    } COMBOBUF;

typedef struct commandBuf
    {
    char cmd[2];
    uint16_t channelNo;
    } COMMANDBUF;

// This is the type of buffer sent from LH to DE to request
//  creation of a configuration channel pair.
// The cmd field gets filled with CREATE_CHANNEL commnand string.
typedef struct configChannelRequest
	{
    char cmd[2];
    uint16_t channelNo;
	uint16_t configPort;  // Port C for this channelNo
	uint16_t dataPort;    // Port F for this channelNo
	} CONFIGBUF;

struct channelBlock
	{
    int subChannelNo;    
    int antennaPort;     // Which DE antenna port (0 or 1 for TangerineSDR) 
    double channelFreq;  // Center frequency for this subchannel
    };

// This is the type of buffer sent from LH to DE to request
// creation of a data channel pair.
// The chCommand field gets filled with the CONFIG_CHANNELS command string.
typedef struct channelBuf
	{
    char chCommand[2];
    uint16_t channelNo;
    char VITA_type[2];      // Which version of VITA is data to use
    int activeSubChannels;  // How many subchannels are defined in channelBlock list
    int channelDatarate;    // Data acquisition rate for this channel in samples/sec
    struct channelBlock channelDef[16];
    } CHANNELBUF;


