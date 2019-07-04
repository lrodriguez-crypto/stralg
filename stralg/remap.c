#include "remap.h"
#include <string.h>

// this remap could be a lot easier if I didn't preserve
// the input order, but I prefer to do so for debugging
// purposes. It doesn't cost much anyway
void build_remap_table(struct remap_table *table,
                       const char *string)
{
    unsigned const char *x;
    
    // collect existing characters
    for (x = (unsigned const char *)string; *x; x++) {
        if (table->table[*x] == -1) {
            table->alphabet_size++;
            table->table[*x] = 1;
        }
    }
    
    // now give the alphabet indices numbers in an
    // order that matches the input
    for (int i = 0, char_no = 0; i < 256; i++) {
        if (table->table[i] != -1) {
            table->table[i] = char_no;
            table->rev_table[char_no] = i;
            char_no++;
        }
    }
}


struct remap_table *alloc_remap_table(const char *string)
{
    struct remap_table *table = malloc(sizeof(struct remap_table));
    init_remap_table(table, string);
    return table;
}

void init_remap_table(struct remap_table *table,
                       const char *string)
{
    table->alphabet_size = 1; // we always have zero
    
    // set table intries to -1. This indicates a letter
    // that we haven't seen.
    memset(table->table,     -1, sizeof(table->table));
    memset(table->rev_table, -1, sizeof(table->rev_table));
    // sentinel is always sentinel
    table->table[0] = 0;
    table->rev_table[0] = 0;
    
    build_remap_table(table, string);
}

void dealloc_remap_table(struct remap_table *table)
{
    // we haven't allocated any resources
}

void free_remap_table(struct remap_table *table)
{
    free(table);
}


char *remap_between(char *output,
                    const char *from,
                    const char *to,
                    struct remap_table *table)
{
    char *x = output;
    const char *y = from;
    for (; y != to; ++y, ++x) {
        *x = table->table[(unsigned char)*y];
        if (*x < 0) return 0;
    }
    return x;
}

char *remap_between0(char *output,
                    const char *from,
                    const char *to,
                    struct remap_table *table)
{
    char *x = remap_between(output, from, to, table);
    if (!x) return 0;
    *x = '\0';
    return x + 1;
}

char *remap(char *output, const char *input,
           struct remap_table *table)
{
    // since we map up to length + 1, we automatically
    // get a zero sentinel (the last character we copy from
    // input. The 'between' versions do not add the
    // sentinels.
    return remap_between(output,
                         input, input + strlen(input) + 1,
                         table);
}

char *rev_remap_between(char *output,
                        const char *from, const char *to,
                        struct remap_table *table)
{
    char *x = output;
    const char *y = from;
    for (; y != to; ++y, ++x) {
        *x = table->rev_table[(unsigned int)*y];
        if (*x < 0) return 0;
    }
    return x;
}

char *rev_remap(char *output, const char *input,
               struct remap_table *table)
{
    return rev_remap_between(output,
                             input, input + strlen(input) + 1,
                             table);
}

char *rev_remap_between0(char *output,
                        const char *from,
                        const char *to,
                        struct remap_table *table)
{
    char *x = rev_remap_between(output, from, to, table);
    if (!x) return 0;
    *x = '\0';
    return x + 1;
}

void write_remap_table(FILE *f, const struct remap_table *table)
{
    fwrite(table, sizeof(struct remap_table), 1, f);
}

void write_remap_table_fname(const char *fname, const struct remap_table *table)
{
    FILE *f = fopen(fname, "wb");
    write_remap_table(f, table);
    fclose(f);
}

struct remap_table *read_remap_table(FILE *f)
{
    struct remap_table *remap_table = malloc(sizeof(struct remap_table));
    fread(remap_table, sizeof(struct remap_table), 1, f);
    return remap_table;
}

struct remap_table *read_remap_table_fname(const char *fname)
{
    FILE *f = fopen(fname, "rb");
    struct remap_table *res = read_remap_table(f);
    fclose(f);
    return res;
}

void print_remap_table(const struct remap_table *table)
{
    printf("0 -> $\n");
    for (unsigned char i = 1; i < table->alphabet_size; ++i) {
        signed char rev = table->rev_table[i];
        signed char back = table->table[rev];
        printf("%d -> %c -> %d\n", i, rev, back);
    }
}

char *backmapped(struct remap_table *table, const char *x)
{
    uint32_t n = strlen(x);
    char *result = malloc(n + 1);
    rev_remap(result, x, table);
    return result;
}

bool identical_remap_tables(const struct remap_table *table1,
                            const struct remap_table *table2)
{
    if (table1->alphabet_size != table2->alphabet_size)
        return false;
    
    for (uint32_t i = 0; i < table1->alphabet_size; ++i) {
        if (table1->table[i] != table2->table[i])
            return false;
    }
    
    return true;
}
