// sound_seg.h
#ifndef SOUND_SEG_H
#define SOUND_SEG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Forward declaration for the sound segment structure.
typedef struct sound_seg sound_seg;

// Allocate and initialize a new empty track.
sound_seg* tr_init();

// Destroy a track and free all associated memory.
void tr_destroy(sound_seg* track);

// Return the length (in samples) of the track.
size_t tr_length(sound_seg* track);

// Read samples from track starting at `pos` into `dest`.
void tr_read(sound_seg* track, int16_t* dest, size_t pos, size_t len);

// Write samples from `src` into track starting at `pos`.
void tr_write(sound_seg* track, const int16_t* src, size_t pos, size_t len);

// Delete a range of samples from the track.
bool tr_delete_range(sound_seg* track, size_t pos, size_t len);

// Identify occurrences of ad within the target track using cross-correlation.
char* tr_identify(const sound_seg* target, const sound_seg* ad);

// Insert a portion from one track (src) into another (dest).
void tr_insert(sound_seg* src_track, sound_seg* dest_track, size_t destpos, size_t srcpos, size_t len);

#endif // SOUND_SEG_H