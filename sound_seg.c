#include "sound_seg.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

// Structure representing a block of audio data.
typedef struct audio_block {
    size_t length;
    uint16_t refcount;
    int16_t *data;
} audio_block;

// Represents a child relationship in the segment tree.
typedef struct segment_child {
    struct segment* child;
    struct segment_child* next;
} segment_child;

// A segment of audio within a track. Can have parent/child relations for shared data.
typedef struct segment {
    size_t offset;
    size_t length;
    struct segment* parent;
    struct segment_child* children;
    struct segment *next;
    audio_block *block;
    uint16_t refcount;
} segment;

// The main structure representing a sound track.
typedef struct sound_seg {
    segment *head;
} sound_seg;

// Initialize the empty sound track.
struct sound_seg* tr_init() {
    struct sound_seg* track = (struct sound_seg*) malloc(sizeof(struct sound_seg));
    if (!track) {
        return NULL;
    }

    track->head = NULL;
    return track;
}

// Free a segment and all associated child structures
void destroy_seg(segment* seg) {
    if (!seg) return;

    segment_child* child = seg->children;
    while (child) {
        segment_child* temp_child = child;
        child = child->next;
        free(temp_child);
    }

    if (seg->block) {
        seg->block->refcount--;
        if (seg->block->refcount == 0) {
            free(seg->block->data);
            free(seg->block);
        }
    }

    free(seg);
}

// Destroy a track and all its segments, releasing memory
void tr_destroy(struct sound_seg* track) {
    if (!track) {
        return;
    }

    while (track->head) {
        segment *to_delete = track->head;
        track->head = to_delete->next;
        destroy_seg(to_delete);
    }

    free(track);
}

// Return the total number of samples in a segment chain
size_t segment_chain_length(segment* head) {
    size_t total = 0;
    while (head) {
        total += head->length;
        head = head->next;
    }
    return total;
}

// Return the length (number of samples) of the track
size_t tr_length(struct sound_seg* track) {
    if (!track) {
        return 0;
    }

    return segment_chain_length(track->head);
}

// Read samples from the track into the provided destination buffer
// Starting at position `pos`, copy up to `len` samples
void tr_read(struct sound_seg* track, int16_t* dest, size_t pos, size_t len) {
    size_t track_len = tr_length(track);
    if (!track || !dest || pos >= track_len || len == 0) {
        return;
    }

    size_t available = track_len - pos;
    size_t to_read = (len < available) ? len : available;

    segment* seg = track->head;
    size_t seg_start = 0;
    size_t dest_offset = 0;

    while (seg && to_read > 0) {
        size_t seg_end = seg_start + seg->length;

        if (pos < seg_end) {
            size_t local_offset = (pos > seg_start) ? (pos - seg_start) : 0;
            size_t readable = seg->length - local_offset;
            size_t chunk = (to_read < readable) ? to_read : readable;

            memcpy(dest + dest_offset,
                   seg->block->data + seg->offset + local_offset,
                   chunk * sizeof(int16_t));

            pos += chunk;
            dest_offset += chunk;
            to_read -= chunk;
        }

        seg_start += seg->length;
        seg = seg->next;
    }
}

// Return the last segment in a segment chain
segment* find_segment_tail(segment* head) {
    while (head && head->next) {
        head = head->next;
    }
    return head;
}

// Append a new segment to the end of the track with newly allocated audio block
void append_segment(struct sound_seg* track, const int16_t* src, size_t len) {
    if (!track || !src || len == 0) {
        return;
    }

    audio_block* block = (audio_block*) malloc(sizeof(audio_block));
    if (!block) return;

    block->data = (int16_t*) malloc(len * sizeof(int16_t));
    if (!block->data) {
        free(block);
        return;
    }

    memcpy(block->data, src, len * sizeof(int16_t));
    block->length = len;
    block->refcount = 1;

    segment* seg = (segment*) malloc(sizeof(segment));
    if (!seg) {
        free(block->data);
        free(block);
        return;
    }

    seg->block = block;
    seg->offset = 0;
    seg->length = len;
    seg->parent = NULL;
    seg->children = NULL;
    seg->refcount = 0;
    seg->next = NULL;

    if (!track->head) {
        track->head = seg;
    } 
    else {
        segment* tail = find_segment_tail(track->head);
        tail->next = seg;
    }
}

// Write data from `src` into the track at position `pos`, up to `len` samples
// If the write position exceeds track length, append new segments
void tr_write(struct sound_seg* track, const int16_t* src, size_t pos, size_t len) {
    size_t track_len = tr_length(track);
    if (!track || !src || len == 0) {
        return;
    }

    segment* seg = track->head;
    size_t seg_start = 0;
    size_t src_offset = 0;

    while (seg && pos < track_len && len > 0) {
        size_t seg_end = seg_start + seg->length;

        if (pos < seg_end) {
            size_t local_offset = (pos > seg_start) ? (pos - seg_start) : 0;
            size_t available = seg->length - local_offset;
            size_t to_write = (len < available) ? len : available;

            memcpy(seg->block->data + seg->offset + local_offset,
                   src + src_offset, to_write * sizeof(int16_t));

            pos += to_write;
            src_offset += to_write;
            len -= to_write;
        }

        seg_start += seg->length;
        seg = seg->next;
    }

    if (len > 0) {
        append_segment(track, src + src_offset, len);
    }
}

// Check if a segment can be deleted
bool can_delete_segment(segment* seg) {
    if (!seg || !seg->block) return false;

    if (seg->refcount > 0) {
        return false;
    }

    return true;
}

// Check if all segments are deletable
bool can_delete_range(struct sound_seg* track, size_t pos, size_t len) {
    size_t track_len = tr_length(track);
    if (!track || pos >= track_len || len == 0) {
        return false;
    }

    if (pos + len > track_len) {
        len = track_len - pos;
    }

    segment* seg = track->head;
    size_t seg_start = 0;

    while (seg) {
        size_t seg_end = seg_start + seg->length;

        if (seg_end > pos && seg_start < pos + len) {
            if (!can_delete_segment(seg)) {
                return false;
            }
        }

        seg_start += seg->length;
        seg = seg->next;
    }

    return true;
}

// Add child reference to parent's children list
void add_child_to_parent(segment* parent, segment* child) {
    if (!parent || !child) return;

    segment_child* new_child = (segment_child*) malloc(sizeof(segment_child));
    if (!new_child) return;

    new_child->child = child;
    new_child->next = parent->children;
    parent->children = new_child;
}

// Remove a segment from its parent's child list
void remove_child_from_parent(segment* seg) {
    if (!seg || !seg->parent) return;

    segment_child* prev_child = NULL;
    segment_child* child = seg->parent->children;

    while (child) {
        if (child->child == seg) {
            if (prev_child) {
                prev_child->next = child->next;
            } 
            else {
                seg->parent->children = child->next;
            }
            free(child);
            break;
        }
        prev_child = child;
        child = child->next;
    }
}

// Split a segment into two parts at `cut_down` offset
void split_segment(segment* seg, size_t cut_down, segment* left_parent, segment* right_parent) {
    if (cut_down == 0 || cut_down == seg->length) {
        return;
    }

    segment* new_seg = (segment*) malloc(sizeof(segment));
    if (!new_seg) return;

    *new_seg = *seg;
    new_seg->length -= cut_down;
    new_seg->offset += cut_down;
    seg->block->refcount += 1;
    seg->length = cut_down;
    seg->next = new_seg;

    seg->parent = left_parent;
    new_seg->parent = right_parent;

    if (left_parent) {
        add_child_to_parent(left_parent, seg);
    }

    if (right_parent) {
        add_child_to_parent(right_parent, new_seg);
    }

    segment_child* children = seg->children;
    seg->children = NULL;
    new_seg->children = NULL;

    segment_child* seg_child = children;
    while (seg_child) {
        split_segment(seg_child->child, cut_down, seg, new_seg);
        seg_child = seg_child->next;
    }

    segment_child* temp_child;
    while (children) {
        temp_child = children;
        children = children->next;
        free(temp_child);
    }    
}

// Ensure a recursive split is performed at the root segment level
void recursive_split(segment* seg, size_t cut_down) {
    while (seg->parent) {
        seg = seg->parent;
    }

    split_segment(seg, cut_down, NULL, NULL);
}

// Delete a range of samples from a track if safe
bool tr_delete_range(struct sound_seg* track, size_t pos, size_t len) {
    size_t track_len = tr_length(track);

    if (!track || pos >= track_len || len == 0) {
        return false;
    }

    if (!can_delete_range(track, pos, len)) {
        return false;
    }

    if (pos + len > track_len) {
        len = track_len - pos;
    }

    segment* seg = track->head;
    segment* prev = NULL;
    size_t cur_pos = 0;
    
    while (seg) {
        size_t seg_start = cur_pos;
        size_t seg_end = seg_start + seg->length;

        segment* to_delete = NULL;

        if (pos < seg_end && pos + len > seg_start) {
            size_t del_start = (pos > seg_start) ? (pos - seg_start) : 0;
            size_t del_end = ((pos + len) < seg_end) ? (pos + len - seg_start) : seg->length;
    
            recursive_split(seg, del_end);
            recursive_split(seg, del_start);

            while (prev && prev->next != seg) {
                prev = prev->next;
            }
    
            if (del_start == 0) {
                if (prev) {
                    prev->next = seg->next;
                } 
                else {
                    track->head = seg->next;
                }
                to_delete = seg;
            } 
        }
        if (to_delete) {
            if (to_delete->parent) {
                to_delete->parent->refcount -= 1;
            }
            cur_pos += seg->length;
            seg = seg->next;
            remove_child_from_parent(to_delete);
            destroy_seg(to_delete);
        }
        else {
            cur_pos += seg->length;
            prev = seg;
            seg = seg->next;
        }
    }
        
    return true;
}

// Search for segments in `target` that match the given `ad` segment using correlation
char* tr_identify(const struct sound_seg* target, const struct sound_seg* ad) {
    size_t target_len = tr_length((struct sound_seg*)target);
    size_t ad_len = tr_length((struct sound_seg*)ad);
    if (!target || !ad || target_len == 0 || ad_len == 0 || 
        ad_len > target_len || !target->head || !ad->head) {
        char* empty = malloc(1);
        if (empty) {
            empty[0] = '\0';
        }
        return empty;
    }
    
    double reference = 0.0;

    const int16_t* target_data = target->head->block->data;
    const int16_t* ad_data = ad->head->block->data;
    
    for (size_t i = 0; i < ad_len; i++) {
        reference += (double)ad_data[i] * (double)ad_data[i];
    }
    reference = reference / ad_len;
    
    char* results = NULL;
    size_t result_buffer_size = 0;
    size_t result_length = 0;
    bool first_result = true;
    
    for (size_t pos = 0; pos + ad_len <= target_len; pos++) {
        double correlation = 0.0;
        const int16_t* target_segment = target_data + pos;
        
        for (size_t i = 0; i < ad_len; i++) {
            correlation += (double)target_segment[i] * (double)ad_data[i];
        }
        correlation = correlation / ad_len;
        
        if (correlation >= 0.95 * reference) {
            size_t end_pos = pos + ad_len - 1;
            char temp_buffer[64];
            int length;
            
            if (first_result) {
                length = snprintf(temp_buffer, sizeof(temp_buffer), "%zu,%zu", pos, end_pos);
                first_result = false;
            } else {
                length = snprintf(temp_buffer, sizeof(temp_buffer), "\n%zu,%zu", pos, end_pos);
            }
            
            size_t new_length = result_length + length;
            if (new_length >= result_buffer_size) {
                size_t new_size = result_buffer_size == 0 ? 128 : result_buffer_size * 2;
                while (new_size <= new_length) {
                    new_size *= 2;
                }
                
                char* new_buffer = (char*)realloc(results, new_size);
                if (!new_buffer) {
                    free(results);

                    char* empty = malloc(1);
                    if (empty) {
                        empty[0] = '\0';
                    }
                    return empty;
                }
                
                results = new_buffer;
                result_buffer_size = new_size;
            }
            
            memcpy(results + result_length, temp_buffer, length);
            result_length += length;
            results[result_length] = '\0';
            
            pos = end_pos;
        }
    }
    
    if (!results) {
        char* empty = malloc(1);
        if (empty) {
            empty[0] = '\0';
        }
        return empty;
    }
    
    return results;
}

// Extract a shared segment chain from src_track starting at srcpos with length len
segment* extract_segment_slice(struct sound_seg* src_track, size_t srcpos, size_t len) {
    size_t track_len = tr_length(src_track);
    if (!src_track || len == 0 || srcpos + len > track_len) {
        return NULL;
    }

    segment* seg = src_track->head;
    size_t cur_pos = 0;
    segment* result_head_final = NULL;

    while (seg && len > 0) {
        size_t seg_start = cur_pos;
        size_t seg_end = seg_start + seg->length;

        if (srcpos >= seg_end) {
            cur_pos += seg->length;
            seg = seg->next;
            continue;
        }

        size_t local_start = (srcpos > seg_start) ? (srcpos - seg_start) : 0;
        size_t available = seg->length - local_start;
        size_t take = (len < available) ? len : available;

        recursive_split(seg, local_start + take);
        recursive_split(seg, local_start);

        if (local_start == 0) {
            segment* new_seg = (segment*) malloc(sizeof(segment));
            if (!new_seg) return NULL;

            *new_seg = *seg;
            add_child_to_parent(seg, new_seg);
            new_seg->children = NULL;
            new_seg->parent = seg;
            new_seg->refcount = 0;
            new_seg->next = NULL;
            seg->refcount +=1;
            seg->block->refcount += 1;

            segment* tail = find_segment_tail(result_head_final);
            if (!tail) {
                result_head_final = new_seg;
            }
            else {
                tail->next = new_seg;
            }

            len -= take;
            srcpos += take;
        }
        cur_pos += seg->length;
        seg = seg->next;
    }
    return result_head_final;
}

// Insert the given segment chain into the track at destpos
bool insert_segment_chain(struct sound_seg* track, size_t destpos, segment* insert_chain) {
    size_t track_len = tr_length(track);
    if (!track || !insert_chain || destpos > track_len) {
        return false;
    }

    segment* seg = track->head;
    segment* prev = NULL;
    size_t cur_pos = 0;

    if (destpos == 0) {
        segment* tail = find_segment_tail(insert_chain);
        tail->next = track->head;
        track->head = insert_chain;
        return true;
    }

    while (seg) {
        size_t seg_start = cur_pos;
        size_t seg_end = seg_start + seg->length;

        if (destpos <= seg_end) {
            size_t local_offset = destpos - seg_start;

            if (local_offset == 0) {
                segment* tail = find_segment_tail(insert_chain);
                if (prev) {
                    prev->next = insert_chain;
                } 
                else {
                    track->head = insert_chain;
                }
                tail->next = seg;
            } 
            else if (local_offset == seg->length) {
                segment* tail = find_segment_tail(insert_chain);
                tail->next = seg->next;
                seg->next = insert_chain;
            } 
            else {
                recursive_split(seg, local_offset);
                segment* tail = find_segment_tail(insert_chain);
                tail->next = seg->next;
                seg->next = insert_chain;
            }
            return true;
        }

        cur_pos += seg->length;
        prev = seg;
        seg = seg->next;
    }

    segment* tail = find_segment_tail(track->head);
    if (tail) {
        tail->next = insert_chain;
    } 
    else {
        track->head = insert_chain;
    }
    return true;
}

// Insert a portion from src_track into dest_track
void tr_insert(struct sound_seg* src_track,
               struct sound_seg* dest_track,
               size_t destpos, size_t srcpos, size_t len) {
    size_t src_len = tr_length(src_track);
    size_t dest_len = tr_length(dest_track);  
    
    if (!src_track || !dest_track || len == 0) {
        return;
    }

    if (srcpos + len > src_len || destpos > dest_len) {  
        return;
    }

    segment* ref_chain = extract_segment_slice(src_track, srcpos, len);
    if (!ref_chain) {
        return;
    }

    insert_segment_chain(dest_track, destpos, ref_chain);
} 