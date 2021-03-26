#include "modemconfig.h"
#include "fec.h"
#include "filter.h"
#include "newqpskrx.h"
#include <stdio.h>
#include <stdint.h>
#include <alloca.h>
#include <string.h>
#ifdef LOOPBACK
#include <alsa/asoundlib.h>
#endif
#include <sndfile.h>
#include <stdlib.h>
#include <stdarg.h>
#include "header.h"
#include <arpa/inet.h>
#include "tbl.h"
#include <zlib.h>

#include "test.h"

#define SAMPLERATE(x)    ((float)(x)*SymbolLen/DataCarriers/SymbolBits)

uint8_t pktput_data[sizeof(struct packet)];
int pktput_ptr = 0;

void carrier_dropped(int chan);

int read_file = 0;
#ifdef LOOPBACK
snd_pcm_t *pcm_handle;
#endif
int sample_pos = 0;

void resetsample(void)
{
  sample_pos = 0;
}

static complex getsample(struct rxstate *s, int chan)
{
	int16_t samples[SymbolLen];
	complex csamples[SymbolLen];
	int i;

	if (s->bufptr >= s->buflen) {
              memcpy(samples, samples_sep[chan] + sample_pos, SymbolLen * 2);
              sample_pos += SymbolLen;

              s->rxphase = (s->rxphase + SymbolLen) & 0xffff;

              for (i = 0; i < SymbolLen; i++) {
                      //printf("sample %d\n", samples[i]);
                      csamples[i].re = samples[i] * (1.0 / 32768.0);
                      csamples[i].im = csamples[i].re;
              }

              s->buflen = filter(&s->filt, csamples, s->rxbuf);
              s->bufptr = 0;
	}
	return s->rxbuf[s->bufptr++];
}

#define MIN_SYNC_BYTES 2
int synced = 0;

void init_rx(struct rxstate *s, int bps, int fec, int samplerate, int chan)
{
  memset(s, 0, sizeof(*s));
  s->bps = bps;
  s->srate = SAMPLERATE(s->bps);
  s->fec.inlv = 8;
  s->fec.feclevel = fec;
  s->mintune = 8;
  s->minsync = 8;
  s->chan = (void *)(long)chan;
  float rate = SAMPLERATE(s->bps) / samplerate;
  logprintf(2, "samplerate %d rate %f\n", samplerate, rate);
  float f1 = 0.1 * rate;
  float f2 = 0.9 * rate;
  init_tbl();
  init_fec(&s->fec);
  init_filter(&s->filt, rate, f1, f2);
  init_newqpskrx(s);
  s->rxphase = 0;
}

#define SAMP_READ (samples + opt.samplerate * 1)

int recv_ok = 0;

int rx_main(int samples)
{
  int i, j, err;
  struct rxstate s;

#ifdef LOOPBACK
  if ((err = snd_pcm_open(&pcm_handle, opt.adev, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
    fprintf(stderr, "Capture open error: %s\n", snd_strerror(err));
    exit(64);
  }
  if ((err = snd_pcm_set_params(pcm_handle,
    SND_PCM_FORMAT_S16,
    SND_PCM_ACCESS_RW_INTERLEAVED,
    opt.channels,	/* XXX: more channels */
    opt.samplerate,
    0,
    500000)) < 0) {
    fprintf(stderr, "Capture params error: %s\n", snd_strerror(err));
    exit(65);
  }

  memset(samples_int, 0, sizeof(samples_int));
  logprintf(2, "reading %d samples\n", SAMP_READ);
  if ((err = snd_pcm_readi(pcm_handle, samples_int, SAMP_READ)) != SAMP_READ) {
    fprintf(stderr, "snd_pcm_readi error %s\n", snd_strerror(err));
    exit(66);
  }
#else
  SF_INFO sfinfo;
  memset (&sfinfo, 0, sizeof (sfinfo));
  sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
  SNDFILE* in = sf_open(opt.out_file, SFM_READ, &sfinfo);
  if (!in) {
    fprintf(stderr, "Input file open error\n");
    exit(64);
  }
  int ret;
  if ((ret = sf_read_short(in, samples_int, SAMP_READ)) != SAMP_READ) {
    fprintf(stderr, "sf_read_short returned %d\n", ret);
    exit(66);
  }
  sf_close(in);
#endif

  logprintf(2, "read input\n");
  for (j = 0; j < opt.channels; ++j) {
    for (i = 0; i < SAMP_READ; ++i) {
      samples_sep[j][i] = samples_int[i * opt.channels + j];
    }
  }

  err = 0;
  for (j = 0; j < opt.channels; ++j) {
    recv_ok = 0;
    printf("Channel %d: ", j); fflush(stdout);
    init_rx(&s, opt.bps, opt.fec, opt.samplerate, j);
    
    int count = 0;
    
    complex buf[SymbolLen / 2];
    
    synced = 0;
    resetsample();
    while (sample_pos < SAMP_READ) {
      int i;
      if (s.skip)
        logprintf(2, "skipping %d\n", s.skip);
      for (i = 0; i < s.skip; i++)
        getsample(&s, j);
      s.skip = 0;
      for (i = 0; i < SymbolLen / 2; i++)
        buf[i] = getsample(&s, j);
      newqpskrx(&s, buf);
      if (synced == 0)
        s.shreg = 0;
      count += SymbolLen;
    }

    if (recv_ok)
      printf("OK\n");
    else {
      printf("FAIL\n");
      ++err;
    }

    logprintf(2, "total samples %d (%d s)\n", count, count / opt.samplerate);
  }

  return err;
}


int pktput(struct modemchannel *chan, unsigned char *data, unsigned int len)
{
  int i;
  for (i = 0; i < len; i++) {
        if (synced < MIN_SYNC_BYTES || (synced < NUM_SYNC_BYTES && data[i] == 0xff)) {
          if (data[i] == 0xff) {
            logprintf(2, "synced!\n");
            synced++;
          }
          else {
            if (synced)
              logprintf(2, "desynced (%02X)\n", data[i]);
            synced = 0;
          }
        }
        else {
          synced = NUM_SYNC_BYTES;
          logprintf(5, "pktput %02x (%d)\n", data[i], data[i]);
          if (pktput_ptr < sizeof(pktput_data)) {
            pktput_data[pktput_ptr++] = data[i];
          }
          if (opt.verbose >= 5)
            putchar(data[i]);
        }
  }
  return len;
}

void logvprintf(unsigned int level, const char *fmt, va_list args)
{
  if (level <= opt.verbose)
    vfprintf(stderr, fmt, args);
  if (!level)
    exit(96);
}

void logprintf(unsigned int level, const char *fmt, ...)
{
        va_list args;

        va_start(args, fmt);
        logvprintf(level, fmt, args);
        va_end(args);
}

void parse_packet(int chan)
{
  struct packet *pkt = (struct packet *)pktput_data;
  if (memcmp(pkt->header.magic, "STRP", 4) != 0) {
    logprintf(2, "bad packet magic %x%x%x%x\n", pkt->header.magic[0], pkt->header.magic[1], pkt->header.magic[2], pkt->header.magic[3]);
    return;
  }

  pkt->header.len = ntohs(pkt->header.len);
  pkt->header.adler32 = ntohl(pkt->header.adler32);
  pkt->header.offset = ntohs(pkt->header.offset);
  pkt->header.file_id = ntohl(pkt->header.file_id);
  
  if (pkt->header.len > 1024) {
    logprintf(2, "bad packet length (%d)\n", pkt->header.len);
    return;
  }

  uint32_t adler = adler32(0, pkt->data, pkt->header.len);
  if (pkt->header.adler32 != adler) {
    logprintf(2, "bad checksum (%x vs %x)\n", pkt->header.adler32, adler);
    return;
  }

  if (!memcmp(pkt->data, msg[chan], strlen(msg[chan]))) {
    logprintf(1, "message correct (%s) ", pkt->data);
    recv_ok = 1;
  } else {
    logprintf(1, "message incorrect (%s) ", pkt->data);
  }
  memset(pktput_data, 0, sizeof(pktput_data));
}

void carrier_dropped(int chan)
{
  synced = 0;
  pktput_ptr = 0;
  parse_packet(chan);
}
void pktsetdcd(struct modemchannel *chan, int dcd)
{
  logprintf(5, "dcd %d chan %p\n", dcd, chan);
  if (!dcd) {
    carrier_dropped((long)chan);
  }
}
