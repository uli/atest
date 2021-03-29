#define MAX_CHANNELS 2
#define MAX_SAMPLES 1000000

extern int16_t samples_sep[MAX_CHANNELS][MAX_SAMPLES];
extern int16_t samples_int[MAX_CHANNELS*MAX_SAMPLES];
extern char *msg[MAX_CHANNELS];

int rx_main(int samples);

struct options_t {
  int bps;
  int samplerate;
  int fec;
  int channels;
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
