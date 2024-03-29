#define MAX_CHANNELS 5
#define MAX_SAMPLES 1000000
#define MSG_LEN 67

extern int16_t samples_sep[MAX_CHANNELS][MAX_SAMPLES];
extern int16_t samples_int[MAX_CHANNELS*MAX_SAMPLES];
extern char msg[MAX_CHANNELS][MSG_LEN + 1];

int rx_main(int samples);

struct options_t {
  int bps;
  int samplerate;
  int fec;
  int channels;
  char *channel_ids;
  int verbose;
#ifdef LOOPBACK
  char *adev;
#else
  char *out_file;
  int samplesize;
  int read;
  int write;
#endif
};

extern struct options_t opt;

void logprintf(unsigned int level, const char *fmt, ...);
