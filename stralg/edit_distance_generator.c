
#include "edit_distance_generator.h"
#include "cigar.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

struct recursive_constant_data {
    const char *buffer_front;
    const char *cigar_front;
    const char *alphabet;
    char *simplify_cigar_buffer;
};

static void recursive_generator(const char *pattern, char *buffer, char *cigar,
                                int max_edit_distance,
                                struct recursive_constant_data *data,
                                edits_callback_func callback,
                                void *callback_data)
{
    if (*pattern == '\0') {
        // no more pattern to match ... terminate the buffer and call back
        *buffer = '\0';
        *cigar = '\0';
        simplify_cigar(data->cigar_front, data->simplify_cigar_buffer);
        callback(data->buffer_front, data->simplify_cigar_buffer, callback_data);

    } else if (max_edit_distance == 0) {
        // we can't edit any more, so just move pattern to buffer and call back
        size_t rest = strlen(pattern);
        for (size_t i = 0; i < rest; ++i) {
            buffer[i] = pattern[i];
            //cigar[i] = '=';
            cigar[i] = 'M';
        }
        buffer[rest] = cigar[rest] = '\0';
        simplify_cigar(data->cigar_front, data->simplify_cigar_buffer);
        callback(data->buffer_front, data->simplify_cigar_buffer, callback_data);

    } else {
        // --- time to recurse --------------------------------------
        // deletion
        *cigar = 'I';
        recursive_generator(pattern + 1, buffer, cigar + 1,
                            max_edit_distance - 1, data,
                            callback, callback_data);
        // insertion
        for (const char *a = data->alphabet; *a; a++) {
            *buffer = *a;
            *cigar = 'D';
            recursive_generator(pattern, buffer + 1, cigar + 1,
                                max_edit_distance - 1, data,
                                callback, callback_data);
        }
        // match / substitution
        for (const char *a = data->alphabet; *a; a++) {
            if (*a == *pattern) {
                *buffer = *a;
                //*cigar = '=';
                *cigar = 'M';
                recursive_generator(pattern + 1, buffer + 1, cigar + 1,
                                    max_edit_distance, data,
                                    callback, callback_data);
            } else {
                *buffer = *a;
                //*cigar = 'X';
                *cigar = 'M';
                recursive_generator(pattern + 1, buffer + 1, cigar + 1,
                                    max_edit_distance - 1, data,
                                    callback, callback_data);
            }
        }
    }
}

void generate_all_neighbours(const char *pattern,
                             const char *alphabet,
                             int max_edit_distance,
                             edits_callback_func callback,
                             void *callback_data)
{
    size_t n = strlen(pattern) + max_edit_distance + 1;
    char buffer[n];
    char cigar[n], cigar_buffer[n];
    struct recursive_constant_data data = { buffer, cigar, alphabet, cigar_buffer };
    recursive_generator(pattern, buffer, cigar, max_edit_distance, &data,
                        callback, callback_data);
}

struct edit_iter_frame {
    const char *pattern_front;
    char *buffer_front;
    char *cigar_front;
    int max_dist;
    struct edit_iter_frame *next;
};

// I am heap allocating the links in the
// stack here. It might be faster to have an
// allocation pool.
static struct edit_iter_frame *
push_edit_iter_frame(
    const char *pattern_front,
    char *buffer_front,
    char *cigar_front,
    int max_dist,
    struct edit_iter_frame *next
) {
    struct edit_iter_frame *frame =
        malloc(sizeof(struct edit_iter_frame));
    frame->pattern_front = pattern_front;
    frame->buffer_front = buffer_front;
    frame->cigar_front = cigar_front,
    frame->max_dist = max_dist;
    frame->next = next;
    return frame;
}

void edit_init_iter(
    const char *pattern,
    const char *alphabet,
    int max_edit_distance,
    struct edit_iter *iter
) {
    size_t n = strlen(pattern) + max_edit_distance + 1;

    iter->pattern = pattern;
    iter->alphabet = alphabet;

    iter->buffer = malloc(n); iter->buffer[n - 1] = '\0';
    iter->cigar = malloc(n);  iter->cigar[n - 1] = '\0';
    iter->simplify_cigar_buffer = malloc(n);

    iter->frames = push_edit_iter_frame(
        iter->pattern,
        iter->buffer,
        iter->cigar,
        max_edit_distance,
        0
    );
}

bool edit_next_pattern(
    struct edit_iter *iter,
    struct edit_iter_result *result
) {
    assert(iter);
    assert(result);

    if (iter->frames == 0) return false;

    // pop top frame
    struct edit_iter_frame *frame = iter->frames;
    iter->frames = frame->next;

    const char *pattern = frame->pattern_front;
    char *buffer = frame->buffer_front;
    char *cigar = frame->cigar_front;

    printf("fronts:\n");
    printf("pattern: %p\n", pattern);
    printf("pattern: %s\n", pattern);
    printf("buffer: %s\n", buffer);
    printf("cigar: %s\n", cigar);
    printf("and max dist %d\n", frame->max_dist);
    printf("--------------------\n");


    if (*pattern == '\0') {
        // no more pattern to match ... terminate the buffer and call back
        *buffer = '\0';
        *cigar = '\0';
        simplify_cigar(iter->cigar, iter->simplify_cigar_buffer);
        result->pattern = iter->buffer;
        result->cigar = iter->simplify_cigar_buffer;

    } else if (frame->max_dist == 0) {
        // we can't edit any more, so just move pattern to buffer and call back
        size_t rest = strlen(pattern);
        for (size_t i = 0; i < rest; ++i) {
            buffer[i] = pattern[i];
            //cigar[i] = '=';
            cigar[i] = 'M';
        }
        buffer[rest] = cigar[rest] = '\0';
        simplify_cigar(iter->cigar, iter->simplify_cigar_buffer);
        result->pattern = iter->buffer;
        result->cigar = iter->simplify_cigar_buffer;

    } else {
        // --- time to recurse --------------------------------------
        // deletion
        *cigar = 'I';

        // FIXME
#if 0
        recursive_generator(pattern + 1, buffer, cigar + 1,
                            max_edit_distance - 1, data,
                            callback, callback_data);
#endif

        iter->frames = push_edit_iter_frame(
            frame->pattern_front + 1,
            frame->buffer_front,
            frame->cigar_front + 1,
            frame->max_dist - 1,
            0
        );
        printf("recurse!\n");
        edit_next_pattern(iter, result); // recurse...

        // FIXME: how do I schedule the other recursions???
    }

#if 0
    } else {
        // --- time to recurse --------------------------------------
        // deletion
        *cigar = 'I';
        recursive_generator(pattern + 1, buffer, cigar + 1,
                            max_edit_distance - 1, data,
                            callback, callback_data);
        // insertion
        for (const char *a = data->alphabet; *a; a++) {
            *buffer = *a;
            *cigar = 'D';
            recursive_generator(pattern, buffer + 1, cigar + 1,
                                max_edit_distance - 1, data,
                                callback, callback_data);
        }
        // match / substitution
        for (const char *a = data->alphabet; *a; a++) {
            if (*a == *pattern) {
                *buffer = *a;
                //*cigar = '=';
                *cigar = 'M';
                recursive_generator(pattern + 1, buffer + 1, cigar + 1,
                                    max_edit_distance, data,
                                    callback, callback_data);
            } else {
                *buffer = *a;
                //*cigar = 'X';
                *cigar = 'M';
                recursive_generator(pattern + 1, buffer + 1, cigar + 1,
                                    max_edit_distance - 1, data,
                                    callback, callback_data);
            }
        }
    }
#endif

    free(frame);
    return true;
}

void edit_dealloc_iter(struct edit_iter *iter)
{
    free(iter->buffer);
    free(iter->cigar);
    free(iter->simplify_cigar_buffer);
}
