OBJS_COMMON = bch.o \
fec.o \
filter.o \
tbl.o \

OBJS_RX = newqpskrx.o test_rx.o
OBJS_TX = newqpsktx.o test_tx.o

OBJS_ALL = $(OBJS_COMMON) $(OBJS_RX) $(OBJS_TX)

#CC=arm-linux-gnueabihf-gcc
CFLAGS = -MMD -O2 -g -Wall

LIBS = -lz -lasound -lm

all: atest

atest: $(OBJS_COMMON) $(OBJS_TX) $(OBJS_RX)
	$(CC) -o $@ $^ $(LIBS)

clean:
	rm -f $(OBJS_ALL) $(OBJS_ALL:%.o=%.d) atest

-include $(OBJS_ALL:%.o=%.d)
