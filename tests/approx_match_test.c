
#include <stralg.h>

#include <stdlib.h>
#include <string.h>



#define BUFFER_SIZE 1024
#define PRINT_RESULTS

#if 0
static void split_vectors(string_vector *first,
                          string_vector *second,
                          string_vector *unique_first,
                          string_vector *unique_second)
{
    sort_string_vector(first); sort_string_vector(second);
    
    size_t i = 0, j = 0;
    while (i < first->used && j < second->used) {
        char *first_front = string_vector_get(first, i);
        char *second_front = string_vector_get(second, j);
        int cmp = strcmp(first_front, second_front);
        if (cmp == 0) {
            i++;
            j++;
        } else if (cmp < 0) {
            string_vector_append(unique_first, string_vector_get(first, i));
            i++;
        } else {
            string_vector_append(unique_second, string_vector_get(second, j));
            j++;
        }
    }
    
    if (i == first->used) {
        // copy the last of second to unique_second.
        for (; j < second->used; ++j) {
            string_vector_append(unique_second, string_vector_get(second, j));
        }
    }
    if (j == second->used) {
        // copy the last of first to unique_first.
        for (; i < first->used; ++i) {
            string_vector_append(unique_first, string_vector_get(first, i));
        }
    }
}
#endif


static char *match_string(size_t idx, const char *string, const char *cigar)
{
    char *new_string = malloc(BUFFER_SIZE);
    sprintf(new_string, "%lu %s %s", idx, string, cigar);
    return new_string;
}

static void free_strings(string_vector *vec)
{
    for (int i = 0; i < vec->used; i++) {
        free(string_vector_get(vec, i));
    }
}


#pragma mark Collecting matches functions

static void exact_approach(const char *string, const char *pattern,
                           const char *alphabet,
                           int dist, string_vector *results)
{
    size_t n = strlen(string);
    
    struct edit_pattern edit_pattern;
    struct match match;
    
    struct edit_iter iter;
    init_edit_iter(&iter, pattern, alphabet, dist);
    while (next_edit_pattern(&iter, &edit_pattern)) {
        // skip those cigars that start with deletions
        int dummy; char dummy_str[1000];
        if (sscanf(edit_pattern.cigar, "%dD%s", &dummy, dummy_str) > 1) {
            continue;
        }
        
        size_t m = strlen(edit_pattern.pattern);
        // If the exact matchers work, I can pick any of them.
        // I use the border array search.
        struct border_match_iter match_iter;
        init_border_match_iter(&match_iter, string, n, edit_pattern.pattern, m);
        while (next_border_match(&match_iter, &match)) {
            string_vector_append(results,
                                 match_string(match.pos,
                                              edit_pattern.pattern,
                                              edit_pattern.cigar));
        }
        dealloc_border_match_iter(&match_iter);
    }
    dealloc_edit_iter(&iter);
}

#if 0
static void print_cigar_list(index_list *list, string_vector *patterns)
{
    while (list) {
        size_t idx = unbox_index(list->data);
        printf("[%lu,%s]->", idx, string_vector_get(patterns, idx));
        list = list->next;
    }
    printf("|\n");
}
#endif

static void aho_corasick_approach(const char *string,
                                  const char *pattern,
                                  const char *alphabet,
                                  int dist, string_vector *results)
{
    
    string_vector patterns; init_string_vector(&patterns, 10);
    string_vector cigars;   init_string_vector(&cigars, 10);
    
    struct edit_iter pattern_iter;
    struct edit_pattern edit_pattern;
    
    // get pattern cloud
    init_edit_iter(&pattern_iter, pattern, alphabet, dist);
    while (next_edit_pattern(&pattern_iter, &edit_pattern)) {
        // skip those cigars that start with deletions
        
        int dummy; char dummy_str[1000];
        if (sscanf(edit_pattern.cigar, "%dD%s", &dummy, dummy_str) > 1) {
            continue;
        }
        
        string_vector_append(&patterns, str_copy(edit_pattern.pattern));
        string_vector_append(&cigars, str_copy(edit_pattern.cigar));
    }
    dealloc_edit_iter(&pattern_iter);
    
    // init patter->cigars table -- we need it if there are more than one
    // cigar per pattern (which there often will be)
    index_list *cigar_table[patterns.used];
    struct trie trie;
    init_trie(&trie);
    for (size_t i = 0; i < patterns.used; ++i) {
        char *pattern = string_vector_get(&patterns, i);
        struct trie *node = get_trie_node(&trie, pattern);
        if (node && node->string_label >= 0) {
            // we have a repeated pattern but with a new cigar
            cigar_table[node->string_label] = prepend_index_link(cigar_table[node->string_label], i);
        } else {
            add_string_to_trie(&trie, pattern, i);
            cigar_table[i] = prepend_index_link(0, i);
        }
        
    }
    compute_failure_links(&trie);
    
    size_t pattern_lengths[patterns.used];
    for (size_t i = 0; i < patterns.used; ++i) {
        pattern_lengths[i] = strlen(string_vector_get(&patterns, i));
    }
    
    struct ac_iter ac_iter;
    struct ac_match ac_match;
    
    init_ac_iter(&ac_iter, string, strlen(string), pattern_lengths, &trie);
    while (next_ac_match(&ac_iter, &ac_match)) {
        size_t pattern_idx = ac_match.string_label;
        const char *pattern = string_vector_get(&patterns, pattern_idx);
        // there might be more than one cigar per pattern
        index_list *pattern_cigars = cigar_table[pattern_idx];
        while (pattern_cigars) {
            size_t cigar_index = unbox_index(pattern_cigars->data);
            const char *cigar = string_vector_get(&cigars, cigar_index);
            char *hit = match_string(ac_match.index, pattern, cigar);
            string_vector_append(results, hit);
            pattern_cigars = pattern_cigars->next;
        }
        
    }
    dealloc_ac_iter(&ac_iter);
    
    free_strings(&patterns);
    free_strings(&cigars);
    dealloc_string_vector(&patterns);
    dealloc_string_vector(&cigars);
    dealloc_trie(&trie);
}

static void st_match(struct suffix_tree *st,
                     const char *pattern, const char *string,
                     int edits,
                     string_vector *st_results)
{
    char path_buffer[st->length + 1];
    
    struct st_approx_match_iter iter;
    struct st_approx_match match;
    init_st_approx_iter(&iter, st, pattern, edits);
    while (next_st_approx_match(&iter, &match)) {
        get_path_string(st, match.root, path_buffer);
        path_buffer[match.match_depth] = '\0';
        char *m = match_string(match.match_label, path_buffer, match.cigar);
        string_vector_append(st_results, m);
    }
    
    dealloc_st_approx_iter(&iter);
}

static void bwt_match(struct suffix_array *sa,
                      // the pattern and string are remapped
                      const char *pattern, const char *string,
                      struct remap_table *remap_table,
                      struct bwt_table *bwt_table,
                      int edits,
                      string_vector *bwt_results)
{
    
    // We need this to get the string we actually matched
    // from the remapped stuff.
    // FIXME: check if this is the correct length needed.
    size_t n = 3 * strlen(pattern) + 1;
    char rev_mapped_match[n];
    
    struct bwt_approx_iter iter;
    struct bwt_approx_match match;
    init_bwt_approx_iter(&iter, bwt_table, pattern, edits);
    while (next_bwt_approx_match(&iter, &match)) {
        rev_remap_between0(rev_mapped_match,
                           string + match.position,
                           string + match.position + match.match_length,
                           remap_table);
        char *m = match_string(match.position, rev_mapped_match, match.cigar);
        string_vector_append(bwt_results, m);
    }
    dealloc_bwt_approx_iter(&iter);
}


#pragma mark The testing functions

static void exact_bwt_test(string_vector *exact_results,
                           struct remap_table *remap_table,
                           char *remapped_pattern, char *remapped_string)
{
    struct suffix_array *sa = qsort_sa_construction(remapped_string);
    
    printf("BWT\t");
    struct bwt_table bwt_table;
    init_bwt_table(&bwt_table, sa, remap_table);
    
    string_vector bwt_results;
    init_string_vector(&bwt_results, 10);
    bwt_match(sa, remapped_pattern, remapped_string,
              remap_table, &bwt_table, 0, &bwt_results);
    
    dealloc_remap_table(remap_table);
    dealloc_bwt_table(&bwt_table);
    
    sort_string_vector(&bwt_results);
    
    assert(string_vector_equal(exact_results, &bwt_results));
    free_strings(&bwt_results);
    dealloc_string_vector(&bwt_results);
    printf("OK\n");
    printf("----------------------------------------------------\n");
    
    free_suffix_array(sa);
}

static void test_exact(const char *pattern, const char *string,
                       const char *alphabet)
{
    printf("\n\nTesting exact matching (with approximative matchers)\n");
    printf("Searching for %s in %s\n", pattern, string);
    printf("====================================================\n");
    string_vector exact_results;
    init_string_vector(&exact_results, 10);
    exact_approach(string, pattern, alphabet, 0, &exact_results);
    sort_string_vector(&exact_results);
    
    printf("Aho-Corasick\t");
    string_vector ac_results;
    init_string_vector(&ac_results, 10);
    aho_corasick_approach(string, pattern, "acgt", 0, &ac_results);
    sort_string_vector(&ac_results);
    assert(string_vector_equal(&exact_results, &ac_results));
    free_strings(&ac_results);
    dealloc_string_vector(&ac_results);
    printf("OK\n");
    printf("----------------------------------------------------\n");
    
    printf("Naive suffix tree\t");
    struct suffix_tree *st = naive_suffix_tree(string);
    string_vector st_results;
    init_string_vector(&st_results, 10);
    st_match(st, pattern, string, 0, &st_results);
    sort_string_vector(&st_results);
    assert(string_vector_equal(&exact_results, &st_results));
    free_strings(&st_results);
    dealloc_string_vector(&st_results);
    free_suffix_tree(st);
    printf("OK\n");
    printf("----------------------------------------------------\n");
    
    // FIXME: add searches using the lcp construction
    // FIXME: make all the matches work with the remapped string
    
    
    printf("----------------------------------------------------\n");
    printf("---------------- REMAPPING MAPPERS -----------------\n");
    printf("----------------------------------------------------\n");
    size_t n = strlen(string);
    char remapped_string[n + 1];
    size_t m = strlen(pattern);
    char remapped_pattern[m + 1];
    
    struct remap_table remap_table;
    init_remap_table(&remap_table, string);
    remap(remapped_string, string, &remap_table);
    if (remap(remapped_pattern, pattern, &remap_table)) {
        // we only do these tests if we can remap the pattern.
#if 0 // need to back-map the match for this to work...
        printf("Aho-Corasick (remapped)\t");
        string_vector ac_results;
        init_string_vector(&ac_results, 10);
        aho_corasick_approach(remapped_string, remapped_pattern, "acgt", 0, &ac_results);
        sort_string_vector(&ac_results);
        
        printf("wanted:\n");
        print_string_vector(&exact_results);
        printf("got\n");
        print_string_vector(&ac_results);
        
        assert(string_vector_equal(&exact_results, &ac_results));
        free_strings(&ac_results);
        dealloc_string_vector(&ac_results);
        printf("OK\n");
        printf("----------------------------------------------------\n");
#endif
        
        printf("BWT\t");
        exact_bwt_test(&exact_results, &remap_table, remapped_pattern, remapped_string);
        printf("OK\n");
        printf("----------------------------------------------------\n");
    }
    
    free_strings(&exact_results);
    dealloc_string_vector(&exact_results);
}





static void test_approx(const char *pattern, const char *string,
                        int edits, const char *alphabet)
{
    printf("\n\nTesting approximative matching with %d %s\n",
           edits, (edits == 1) ? "edit" : "edits");
    printf("Searching for %s in %s\n", pattern, string);
    printf("====================================================\n");
    
    string_vector exact_results;
    init_string_vector(&exact_results, 10);
    exact_approach(string, pattern, alphabet, edits, &exact_results);
    sort_string_vector(&exact_results);
    
    string_vector ac_results;
    init_string_vector(&ac_results, 10);
    aho_corasick_approach(string, pattern, alphabet, edits, &ac_results);
    sort_string_vector(&ac_results);
    
    printf("Naive vs Aho-Corasic\t");
    assert(vector_equal(&exact_results, &ac_results));
    free_strings(&exact_results);
    dealloc_vector(&exact_results);
    printf("OK\n");
    printf("----------------------------------------------------\n");
    
    printf("Aho-Corasic vs \"naive suffix\" tree\t");
    struct suffix_tree *st = naive_suffix_tree(string);
    st_print_dot_name(st, st->root, "naive-tree.dot");
    
    string_vector st_results;
    init_string_vector(&st_results, 10);
    st_match(st, pattern, string, edits, &st_results);
    sort_string_vector(&st_results);
    free_suffix_tree(st);
    
    assert(vector_equal(&ac_results, &st_results));
    free_strings(&st_results);
    dealloc_string_vector(&st_results);
    
    printf("OK\n");
    printf("----------------------------------------------------\n");
    
    size_t n = strlen(string);
    char remappe_string[n + 1];
    size_t m = strlen(pattern);
    char remapped_pattern[m + 1];
    
    struct remap_table remap_table;
    init_remap_table(&remap_table, string);
    remap(remappe_string, string, &remap_table);
    
    // we cannot remap all patterns. They might contain characters outside
    // of those in the string. So we check if it is possible and only do the
    // BWT mapping when it is.
    bool can_remap = remap(remapped_pattern, pattern, &remap_table);
    if (can_remap) {
        printf("Aho-Corasic vs BWT.\t");
        
        
        assert(strlen(string) == strlen(remappe_string));
        assert(strlen(pattern) == strlen(remapped_pattern));
        
        struct suffix_array *sa = qsort_sa_construction(remappe_string);
        
        struct bwt_table bwt_table;
        init_bwt_table(&bwt_table, sa, &remap_table);
        //print_bwt_table(&bwt_table, sa, &remap_table);
        
        
        string_vector bwt_results;
        init_string_vector(&bwt_results, 10);
        bwt_match(sa, remapped_pattern, remappe_string,
                  &remap_table, &bwt_table, edits, &bwt_results);
        sort_string_vector(&bwt_results);
        
        free_suffix_array(sa);
        
        assert(vector_equal(&ac_results, &bwt_results));
        printf("OK\n");
        printf("----------------------------------------------------\n");
        
        free_strings(&bwt_results);
        dealloc_string_vector(&bwt_results);
    }
    
    free_strings(&ac_results);
    dealloc_string_vector(&ac_results);
}


int main(int argc, char **argv)
{
    char *alphabet = "acgt";
    
    bool exact = true; // run exact tests by default
    if (argc == 2 && strcmp(argv[1], "approx") == 0)
        exact = false;
    
    const char *strings[] = {
        "gacacacag",
        "acacacag",
        "acacaca",
        "acactgaca",
        "acataca"
    };
    size_t no_strings = sizeof(strings) / sizeof(const char *);
    printf("no strings: %lu\n", no_strings);
    const char *patterns[] = {
        "acg", "ac", "a", "g", "c"
    };
    size_t no_patterns = sizeof(patterns) / sizeof(const char *);
    
    if (exact) {
        printf("EXACT MATCHING...\n");
        for (size_t i = 0; i < no_patterns; ++i) {
            for (size_t j = 0; j < no_strings; ++j) {
                printf("%s in %s\n", patterns[i], strings[j]);
                test_exact(patterns[i], strings[j], alphabet);
            }
        }
        printf("DONE\n");
        printf("====================================================\n\n");
    } else {
        
        printf("APPROXIMATIVE MATCHING...\n");
        int edits[] = {
            1, 2
        };
        size_t no_edits = sizeof(edits) / sizeof(int);
        
        for (size_t i = 0; i < no_patterns; ++i) {
            for (size_t j = 0; j < no_strings; ++j) {
                for (size_t k = 0; k < no_edits; ++k) {
                    // avoid patterns that can turn to empty strings.
                    if (strlen(patterns[i]) <= edits[k]) continue;
                    
                    printf("%s in %s with %d edits\n",
                           patterns[i], strings[j], edits[k]);
                    test_approx(patterns[i], strings[j], edits[k], alphabet);
                }
            }
        }
        printf("DONE\n");
        printf("====================================================\n\n");
    }
    
    return EXIT_SUCCESS;
}
