CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lrt -lpthread -lm

SRCS = main.c publisher.c blurrer.c edge_detector.c combiner.c bmp.c utils.c
OBJS = $(SRCS:.c=.o)
EXECS = pipeline publisher blurrer edge_detector combiner

all: $(EXECS)

pipeline: main.o bmp.o utils.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

publisher: publisher.o bmp.o utils.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

blurrer: blurrer.o bmp.o utils.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

edge_detector: edge_detector.o bmp.o utils.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

combiner: combiner.o bmp.o utils.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS) $(EXECS)
	find . -type f -name '*_processed.bmp' -delete
	find . -type f -name '*_blurred.bmp' -delete
	find . -type f -name '*_edges.bmp' -delete
	find . -type f -name '*_filtered.bmp' -delete

.PHONY: all clean