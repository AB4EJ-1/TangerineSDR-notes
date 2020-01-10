/*
de_signals.h
Maps program mnemonics to 2-byte commands to be passed to DE
*/
#define STATUS_INQUIRY      "S?"
#define LED1_ON             "Y1"
#define LED1_OFF            "N1"
#define TIME_INQUIRY        "T?"
#define TIME_STAMP          "TS"
#define DEFINE_CHANNEL      "CH"
#define UNDEFINE_CHANNEL    "UC"
#define FIREHOSE_SERVER     "FH"
#define START_DATA_COLL     "SC"
#define STOP_DATA_COLL      "XC"
#define DEFINE_FT8_CHAN     "FT"
#define START_FT8_COLL      "8S"
#define STOP_FT8_COLL       "8X"
#define LED_SET             "SB"  // in case we need to send a binary LED set byte
#define UNLINK		    "UL"
#define HALT_DE		    "XX"
