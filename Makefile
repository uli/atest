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

#CROSS_COMPILE =arm-linux-gnueabihf-
CC	= $(CROSS_COMPILE)gcc
STRIP	= $(CROSS_COMPILE)strip
CFLAGS = -MMD -O2 -g -Wall

ifneq ($(LOOPBACK),)
CFLAGS += -DLOOPBACK
LIBS = -lz -lasound -lm
else
LIBS = -lz -lsndfile -lm
endif

all: atest

atest: $(OBJS_ALL)
	$(CC) -o $@ $^ $(LIBS)
	$(STRIP) $@

clean:
	rm -f $(OBJS_ALL) $(OBJS_ALL:%.o=%.d) atest

-include $(OBJS_ALL:%.o=%.d)
