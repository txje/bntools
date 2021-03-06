#ifndef __NICK_MAP_H__
#define __NICK_MAP_H__

#include <stdint.h>
#include <string.h>
#include "array.h"

#define MAX_ENZYME_NAME_SIZE 31
#define MAX_REC_SEQ_SIZE 127
#define MAX_CHROM_NAME_SIZE 63
#define MAX_FRAGMENT_NAME_SIZE 63

enum nick_flag {
	NICK_PLUS_STRAND  = 1,  /* nick on plus strand */
	NICK_MINUS_STRAND = 2,  /* nick on minus strand */
};

struct nick {
	int pos;
	unsigned int flag;
};

struct fragment {  /* molecule, contig or chromosome */
	char name[MAX_FRAGMENT_NAME_SIZE + 1];
	int size;  /* in bp */
	array(struct nick) nicks;  /* label positions */
};

struct nick_map {
	array(struct fragment) fragments;

	char enzyme[MAX_ENZYME_NAME_SIZE + 1];
	char rec_seq[MAX_REC_SEQ_SIZE + 1];
};

void nick_map_init(struct nick_map *map);
void nick_map_free(struct nick_map *map);
void nick_map_set_enzyme(struct nick_map *map, const char *enzyme, const char *rec_seq);

struct fragment *nick_map_add_fragment(struct nick_map *map, const char *name);
int nick_map_add_site(struct fragment *f, int pos, unsigned int flag);

#endif /* __NICK_MAP_H__ */
