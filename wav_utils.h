#ifndef WAV_UTILS_H
#define WAV_UTILS_H

#include <stdint.h>
#include <stddef.h>

// Load a WAV file
void wav_load(const char* fname, int16_t* dest);

// Save a buffer of samples as a WAV file.
void wav_save(const char* fname, const int16_t* src, size_t len);

#endif // WAV_UTILS_H
