#!/bin/bash

test -z "$CFLAGS" && CFLAGS="-O2 -g -Wall"

OBJS="bch.o fec.o filter.o tbl.o newqpskrx.o newqpsktx.o test_rx.o test_tx.o"
LIBS="-lz -lm"
CC=${CROSS_COMPILE}gcc

if test -n "$LOOPBACK"; then
  CFLAGS="${CFLAGS} -DLOOPBACK"
  LIBS="${LIBS} -lasound"
else
  LIBS="${LIBS} -lsndfile"
fi

test -e build.ninja && ninja -t clean

cat <<EOT >build.ninja
rule cc
  depfile = \$out.d
  command = $CC -MD -MF \$out.d $CFLAGS -c \$in -o \$out

rule link
  command = $CC -s -o \$out \$in $LIBS

build atest: link build/${OBJS// / build/}
EOT
for o in $OBJS ; do
  echo "build build/${o}: cc ${o%.o}.c" >>build.ninja
done

mkdir -p build
echo run \"ninja\" to build
