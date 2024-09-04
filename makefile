CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lrt -lpthread -lm

SRCS = main.c  applyBlur.c applyEdgeDetection.c publishImage.c bmp.c myutils.c
OBJS = $(SRCS:.c=.o)
EXECS = pipeline

all: $(EXECS)

pipeline: main.o bmp.o myutils.o applyBlur.o applyEdgeDetection.o publishImage.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS) $(EXECS)
	find . -type f -name '*_processed.bmp' -delete
	find . -type f -name '*_blurred.bmp' -delete
	find . -type f -name '*_edges.bmp' -delete
	find . -type f -name '*_filtered.bmp' -delete
	find . -type f -name '*temp.bmp' -delete

.PHONY: all clean