#include <stdint.h>

#define MAX_PACKET_SIZE 1024
#define NUM_SYNC_BYTES 4

struct packet_header {
  uint8_t magic[4];
  uint16_t offset;
  uint16_t len;
  uint32_t file_id;
  uint32_t adler32;
};

struct packet {
  struct packet_header header;
  uint8_t data[MAX_PACKET_SIZE];
};
