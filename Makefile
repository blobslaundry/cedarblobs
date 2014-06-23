CC	= gcc
CFLAGS	= -O0 -Wall

DIR_BLOBS  = ./blobs1.2
CFLAGS    += -I$(DIR_BLOBS)/include_system \
	     -I$(DIR_BLOBS)/include_vencoder \
	     -I$(DIR_BLOBS)/include_vdecoder
DE_LDFLAGS = -lavformat -lvdecoder -lvecore -ldl -L$(DIR_BLOBS)
EN_LDFLAGS = -lavformat -lm -lvencoder -L$(DIR_BLOBS)

WRAPOBJ	= wrapalloc.o
EN_OBJECTS = $(WRAPOBJ) options.o mux.o source.o
DE_OBJECTS = $(WRAPOBJ) options.o demux.o sink.o

all: jpegold.blob h264old.blob encoder.blob decoder.blob

jpegold.blob: $(EN_OBJECTS) jpegencode.o
	$(CC) -o $@ $^ $(EN_LDFLAGS)

h264old.blob: $(EN_OBJECTS) h264encode.o
	$(CC) -o $@ $^ $(EN_LDFLAGS)

encoder.blob: $(EN_OBJECTS) encode.o
	$(CC) -o $@ $^ $(EN_LDFLAGS)

decoder.blob: $(DE_OBJECTS) decode.o
	$(CC) -o $@ $^ $(DE_LDFLAGS)


clean:
	rm *.blob *.o

