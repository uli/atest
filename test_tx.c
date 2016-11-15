#include "modemconfig.h"
#include "fec.h"
#include "filter.h"
#include "newqpsktx.h"
#include <stdio.h>
#include <stdint.h>
#include <alloca.h>
#include <string.h>
#include <stdlib.h>
#include "header.h"
#include <zlib.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include "tbl.h"
#include <alsa/asoundlib.h>
#include <sys/wait.h>
#include <getopt.h>

#include "test.h"

#define SAMPLERATE(x)    ((float)(x)*SymbolLen/DataCarriers/SymbolBits)

FILE *fp;

int send_packet(uint8_t *data, int size, int bps, int fec, int samplerate, int16_t **out);

int16_t samples_sep[MAX_CHANNELS][MAX_SAMPLES];
int16_t samples_int[MAX_CHANNELS*MAX_SAMPLES];

char *msg[MAX_CHANNELS] = {
"0000000000000000000000000000000000000000000000000000000000000000000",
"1111111111111111111111111111111111111111111111111111111111111111111",
};


int init_packet(struct packet *pkt, char *msg)
{
    memset(pkt, 0, sizeof(*pkt));
    memcpy(pkt->header.magic, "STRP", 4);
    pkt->header.file_id = 42;	/* not used */
    pkt->header.offset = 0;
    strcpy((char *)pkt->data, msg);
    int len = strlen(msg);
    pkt->header.adler32 = htonl(adler32(0, pkt->data, len));
    pkt->header.len = htons(len);
    return len;
}

struct options_t opt = {
  "default",
  2500,
  48000,
  1,
  2,
  0
};

void parse_options(int argc, char **argv)
{
  for (;;) {
    int option_index = 0;
    static struct option long_options[] = {
      { "device", required_argument, 0, 'd' },
      { "bps", required_argument, 0, 'b' },
      { "samplerate", required_argument, 0, 's' },
      { "fec", required_argument, 0, 'f' },
      { "channels", required_argument, 0, 'c' },
      { "verbose", no_argument, 0, 'v' },
      { 0, 0, 0, 0 }
    };
    int c = getopt_long(argc, argv, "d:b:s:f:c:v",
                        long_options, &option_index);
    if (c == -1)
      break;
    
    switch (c) {
      case 'd': opt.adev = optarg; break;
      case 'b': opt.bps = atoi(optarg); break;
      case 's': opt.samplerate = atoi(optarg); break;
      case 'f': opt.fec = atoi(optarg); break;
      case 'c': opt.channels = atoi(optarg); break;
      case 'v': ++opt.verbose; break;
      default:
        fprintf(stderr, "unknown option -%c\n", c);
        exit(32);
    }
  }
}

int do_test(void)
{
  int i, j;
  int max_sample_cnt = 0;
  printf("Testing %d channels:\n", opt.channels);
  memset(samples_int, 0, sizeof(samples_int));
  for (i = 0; i < opt.channels; ++i) {
    struct packet pkt;
    int len = init_packet(&pkt, msg[i]);
    int16_t *samples;
    int offset = 0;
    int sample_cnt = 0;

    int n = send_packet((uint8_t *)&pkt, sizeof(struct packet_header) + len, opt.bps, opt.fec, opt.samplerate, &samples);
    for (j = 0; j < n; ++j) {
      samples_int[offset * opt.channels + j * opt.channels + i] = samples[j];
    }
    sample_cnt += n;
    free(samples);
    offset += n;

    if (sample_cnt > max_sample_cnt)
      max_sample_cnt = sample_cnt;
  }

  if (fork() == 0) {
    exit(rx_main(max_sample_cnt));
  }

  snd_pcm_t *out;
  snd_pcm_sframes_t frames;
  int err;
  if ((err = snd_pcm_open(&out, opt.adev, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
    fprintf(stderr, "Playback open error: %s\n", snd_strerror(err));
    exit(67);
  }
  if ((err = snd_pcm_set_params(out,
    SND_PCM_FORMAT_S16,
    SND_PCM_ACCESS_RW_INTERLEAVED,
    opt.channels,
    opt.samplerate,
    0,
    500000)) < 0) {
    fprintf(stderr, "Playback params error: %s\n", snd_strerror(err));
    exit(68);
  }

  usleep(500000);
  logprintf(2, "sending %d\n", max_sample_cnt);
  frames = snd_pcm_writei(out, samples_int, max_sample_cnt + opt.samplerate);
  /* XXX: is this safe? */
  if (frames < 0)
    frames = snd_pcm_recover(out, frames, 0);
  if (frames < 0) {
    fprintf(stderr, "snd_pcm_writei failed: %s\n", snd_strerror(frames));
    exit(69);
  }

  snd_pcm_close(out);

  wait(&err);
  if (!WIFEXITED(err))
    return 97;
  else
    return WEXITSTATUS(err);
}

int main(int argc, char **argv)
{
  
  parse_options(argc, argv);

  return do_test();
}

static int requested = -NUM_SYNC_BYTES;
static uint8_t *pktget_data;
int pktget_ptr = 0;
int pktget_size = 0;

int send_packet(uint8_t *data, int size, int bps, int fec, int samplerate, int16_t **out)
{
  struct txstate s;
  memset(&s, 0, sizeof(s));
  s.saved = -1;
  s.bps = bps;
  s.fec.inlv = 8;
  s.fec.feclevel = fec;
  s.tunelen = 16;
  s.synclen = 16;
  float rate = samplerate / SAMPLERATE(s.bps);
  s.bufsize = rate * SymbolLen + 16;
  logprintf(2, "samplerate %d bps %d rate %f bufsize %d\n", samplerate, bps, rate, s.bufsize);
  float f1 = 0.1;
  float f2 = 0.9;
  init_tbl();
  init_filter(&s.filt, rate, f1, f2);
  init_fec(&s.fec);
  init_newqpsktx(&s);
  requested = -NUM_SYNC_BYTES;
  pktget_data = data;
  pktget_ptr = 0;
  pktget_size = size;
  
  int16_t *samples;
  complex *cbuf;
  samples = alloca(s.bufsize * sizeof(int16_t));
  cbuf = alloca(s.bufsize * sizeof(complex));
  s.txdone = 0;
  int count = 0;
  *out = 0;
  
  while (!s.txdone) {
    int n = newqpsktx(&s, cbuf);
    int i;
    for (i = 0; i < n; i++) {
            samples[i] = (cbuf[i].re + cbuf[i].im) * 32768.0;
            //printf("%d %6d [%f + %f i]\n", i, samples[i], cbuf[i].re, cbuf[i].im);
    }
    
    *out = realloc(*out, (count + n) * 2);
    memcpy((*out) + count, samples, n * 2);
    count += n;
  }
  logprintf(2, "total samples %d (%d s)\n", count, count / samplerate);
  return count;
}

int pktget(struct modemchannel *chan, unsigned char *data, unsigned int len)
{
  if (requested < 0) {
    requested++;
    data[0] = 0xff;
    return 1;
  }
  requested += len;
  //printf("%d bytes requested (%d total)\n", len, requested);
  if (pktget_ptr + len > pktget_size)
    return 0;
  unsigned int i;
  for (i = 0; i < len; i++) {
    data[i] = pktget_data[pktget_ptr++];
  }
  return len;
}
