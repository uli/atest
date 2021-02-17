# Enable to test using a loopback cable connecting the ALSA input and output
# devices. If disabled, sound will be written to/read from a WAV file.
#LOOPBACK = 1

OBJS_COMMON = bch.o \
fec.o \
filter.o \
tbl.o \

OBJS_RX = newqpskrx.o test_rx.o
OBJS_TX = newqpsktx.o test_tx.o

OBJS_ALL = $(OBJS_COMMON) $(OBJS_RX) $(OBJS_TX)

#CC=arm-linux-gnueabihf-gcc
CFLAGS = -MMD -O2 -g -Wall

ifneq ($(LOOPBACK),)
CFLAGS += -DLOOPBACK
LIBS = -lz -lasound -lm
else
LIBS = -lz -lsndfile -lm
endif

all: atest

atest: $(OBJS_COMMON) $(OBJS_TX) $(OBJS_RX)
	$(CC) -o $@ $^ $(LIBS)

clean:
	rm -f $(OBJS_ALL) $(OBJS_ALL:%.o=%.d) atest

-include $(OBJS_ALL:%.o=%.d)
