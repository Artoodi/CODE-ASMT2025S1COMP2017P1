CC = gcc
CFLAGS = -g -Wall -Werror -Wvla -fno-sanitize=all -fsanitize=address -fPIC -std=c11

all: sound_seg.o

sound_seg_tmp.o: sound_seg.c sound_seg.h wav_utils.h
	$(CC) $(CFLAGS) -c sound_seg.c -o sound_seg_tmp.o

wav_utils_tmp.o: wav_utils.c wav_utils.h
	$(CC) $(CFLAGS) -c wav_utils.c -o wav_utils_tmp.o

sound_seg.o: sound_seg_tmp.o wav_utils_tmp.o
	ld -r -o sound_seg.o sound_seg_tmp.o wav_utils_tmp.o

clean:
	rm -f *.o
