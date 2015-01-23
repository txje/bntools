#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <zlib.h>
#include "nick_map.h"
#include "base_map.h"
#include "version.h"

int parse_format_text(const char *s)
{
	if (strcmp(s, "txt") == 0) {
		return FORMAT_TXT;
	} else if (strcmp(s, "tsv") == 0) {
		return FORMAT_TSV;
	} else if (strcmp(s, "bnx") == 0) {
		return FORMAT_BNX;
	} else if (strcmp(s, "cmap") == 0) {
		return FORMAT_CMAP;
	} else {
		return FORMAT_UNKNOWN;
	}
}

static inline int string_begins_as(const char *s, const char *prefix)
{
	return (memcmp(s, prefix, strlen(prefix)) == 0);
}

static inline int skip_to_next_line(gzFile file, char *buf, size_t bufsize)
{
	while (strchr(buf, '\n') == NULL) {
		if (!gzgets(file, buf, bufsize)) return 1;
	}
	return 0;
}

static int seq_match(const char *ref, const char *query, size_t len, int strand)
{
	size_t i;
	assert(strand == STRAND_PLUS || strand == STRAND_MINUS);
	for (i = 0; i < len; ++i) {
		char r = ref[i];
		char q = (strand == STRAND_PLUS ? query[i] : base_to_comp(query[len - i - 1]));
		if ((r & q) != r) {
			return 0;
		}
	}
	return 1;
}

struct buffer {
	char data[MAX_REC_SEQ_SIZE * 2];
	size_t pos;
};

static int process_line(struct nick_map *map, struct nick_list *list,
		const char *line, int base_count, struct buffer *buf)
{
	const char *p;
	int strand;
	int matched;
	for (p = line; *p; ++p) {
		if (isspace(*p)) {
			continue;
		}
		if (buf->pos >= sizeof(buf->data)) {
			memcpy(buf->data, buf->data + sizeof(buf->data) - map->rec_seq_size + 1, map->rec_seq_size - 1);
			buf->pos = map->rec_seq_size - 1;
		}
		++base_count;
		buf->data[buf->pos++] = char_to_base(*p);
		if (buf->pos < map->rec_seq_size) {
			continue;
		}
		for (strand = STRAND_PLUS, matched = 0; strand <= STRAND_MINUS; ++strand) {
			if (matched || seq_match(buf->data + buf->pos - map->rec_seq_size, map->rec_bases, map->rec_seq_size, strand)) {
				int site_pos = base_count - (strand == STRAND_MINUS ? map->nick_offset : (map->rec_seq_size - map->nick_offset));
				if (nick_map_add_site(list, site_pos, strand)) {
					return -ENOMEM;
				}
				matched = map->palindrome;
			}
		}
	}
	return base_count;
}

int nick_map_load_fasta(struct nick_map *map, const char *filename,
		int only_chromosome, int transform_to_number, int verbose)
{
	gzFile file;
	struct nick_list *list = NULL;
	char name[MAX_CHROM_NAME_SIZE] = "";
	struct buffer buf = { };
	int c, ret = 0, base_count = 0;

	if (strcmp(filename, "-") == 0 || strcmp(filename, "stdin") == 0) {
		file = gzdopen(0, "r"); /* stdin */
	} else {
		file = gzopen(filename, "r");
	}
	if (!file) {
		fprintf(stderr, "Error: Can not open FASTA file '%s'\n", filename);
		return 1;
	}

	c = gzgetc(file);
	if (c != '>') {
		fprintf(stderr, "Error: File '%s' is not in FASTA format\n", filename);
		ret = -EINVAL;
		goto out;
	}
	gzungetc(c, file);

	while (!gzeof(file)) {
		char line[256];
		if (!gzgets(file, line, sizeof(line))) break;
		if (line[0] == '>') {
			if (list) {
				list->fragment_size = base_count;
				if (verbose > 0) {
					fprintf(stderr, "%d bp\n", base_count);
				}
			}
			if (only_chromosome) {
				int skip = 1;
				const char *p = line + 1;
				if (memcmp(p, "chr", 3) == 0) {
					p += 3;
				}
				if (p[0] >= '1' && p[0] <= '9' && isspace(p[1])) {
					skip = 0;
				} else if ((p[0] == '1' || p[0] == '2') &&
						(p[1] >= '0' && p[1] <= '9') && isspace(p[2])) {
					skip = 0;
				} else if ((p[0] == 'X' || p[1] == 'Y') && isspace(p[1])) {
					skip = 0;
				}
				if (skip) {
					list = NULL;
					continue;
				}
			}
			if (transform_to_number) {
				static int number = 0;
				snprintf(name, sizeof(name), "%d", ++number);
			} else {
				char *p = line + 1;
				while (*p && !isspace(*p)) ++p;
				*p = '\0';
				snprintf(name, sizeof(name), line + 1);
			}
			if (verbose > 0) {
				fprintf(stderr, "Loading fragment '%s' ... ", name);
			}
			base_count = 0;
			list = nick_map_add_fragment(map, name);
			if (!list) {
				ret = -ENOMEM;
				goto out;
			}
		} else if (list) {
			int n = process_line(map, list, line, base_count, &buf);
			if (n < 0) {
				ret = n;
				goto out;
			}
			base_count = n;
		}
	}
	if (list) {
		list->fragment_size = base_count;
		if (verbose > 0) {
			fprintf(stderr, "%d bp\n", base_count);
		}
	}
out:
	gzclose(file);
	return ret;
}

static inline int to_integer(double x) { return (int)(x + .5); }

static int load_bnx(gzFile file, long long lineNo, struct nick_map *map)
{
	char buf[256];
	char molecule_id[64] = "";
	double length = 0;
	int molecule_length;
	struct nick_list *list = NULL;
	while (!gzeof(file)) {
		++lineNo;
		if (!gzgets(file, buf, sizeof(buf))) break;
		if (buf[0] == '#') continue;
		if (memcmp(buf, "0\t", 2) == 0) { /* molecule info */
			if (sscanf(buf + 2, "%s%lf", molecule_id, &length) != 2) {
				fprintf(stderr, "Error: Invalid format on line %lld\n", lineNo);
				return 1;
			}
			molecule_length = to_integer(length);
			list = nick_map_add_fragment(map, molecule_id);
			list->fragment_size = molecule_length;
			if (skip_to_next_line(file, buf, sizeof(buf))) break;
		} else if (memcmp(buf, "1\t", 2) == 0) { /* label positions */
			char *p = buf + 2;
			for (;;) {
				char *q = p;
				double value;
				while (*q && *q != '\t' && *q != '\n') ++q;
				if (*q == '\0') {
					size_t size = q - p;
					memcpy(buf, p, size);
					if (!gzgets(file, buf + size, sizeof(buf) - size)) break;
					p = buf;
					continue;
				}
				if (sscanf(p, "%lf", &value) != 1) {
					fprintf(stderr, "Error: Failed in reading float value on line %lld\n", lineNo);
					return 1;
				}
				nick_map_add_site(list, to_integer(value), 0);
				if (*q == '\t') {
					p = q + 1;
				} else {
					assert(*q == '\n');
					break;
				}
			}
		} else {
			if (skip_to_next_line(file, buf, sizeof(buf))) break;
		}
	}
	return 0;
}

static int load_cmap(gzFile file, long long lineNo, struct nick_map *map)
{
	char buf[256];
	struct nick_list *list = NULL;
	char lastMapId[32] = "";

	while (!gzeof(file)) {
		++lineNo;
		if (!gzgets(file, buf, sizeof(buf))) break;
		if (buf[0] == '#') {
			if (string_begins_as(buf, "# Nickase Recognition Site 1:")) {
				char *p, *q, *e;
				p = strchr(buf, ':');
				assert(p != NULL);
				++p;
				while (*p && isblank(*p)) ++p;
				q = strchr(p, '/');
				if (q != NULL) {
					*q++ = '\0';
					e = strchr(q, '\n');
					if (e) {
						*e = '\0';
					}
					if (nick_map_set_enzyme(map, p, q)) {
						return 1;
					}
				}
			}
			continue;
		} else {
			char mapId[32];
			int ctgLen;
			int numSites;
			int siteId;
			int labelChannel;
			int position;
			if (sscanf(buf, "%s%d%d%d%d%d", mapId, &ctgLen, &numSites, &siteId, &labelChannel, &position) != 6) {
				fprintf(stderr, "Error: Failed to parse data on line %lld\n", lineNo);
				return -EINVAL;
			}

			if (strcmp(lastMapId, mapId) != 0) {
				list = nick_map_add_fragment(map, mapId);
			}
			assert(list != NULL);

			if (labelChannel == 1) {
				nick_map_add_site(list, position, 0);
			} else {
				assert(labelChannel == 0);
				list->fragment_size = position;
			}
		}
	}
	return 0;
}

static int load_tsv(gzFile file, long long lineNo, struct nick_map *map)
{
	char buf[256];
	struct nick_list *list = NULL;
	char lastName[32] = "";

	while (!gzeof(file)) {
		++lineNo;
		if (!gzgets(file, buf, sizeof(buf))) break;
		if (buf[0] == '#') {
			if (string_begins_as(buf, "##enzyme=")) {
				char *p, *q, *e;
				p = strchr(buf, '=');
				assert(p != NULL);
				++p;
				q = strchr(p, '/');
				if (q != NULL) {
					*q++ = '\0';
					e = strchr(q, '\n');
					if (e) {
						*e = '\0';
					}
					if (nick_map_set_enzyme(map, p, q)) {
						return 1;
					}
				}
			}
			continue;
		} else {
			char name[32];
			int pos;
			char strandText[8];
			int strand;

			if (sscanf(buf, "%s%d%s", name, &pos, strandText) != 3) {
				fprintf(stderr, "Error: Failed to parse data on line %lld\n", lineNo);
				return -EINVAL;
			}

			if (strcmp(strandText, "?") == 0) {
				strand = STRAND_UNKNOWN;
			} else if (strcmp(strandText, "+") == 0) {
				strand = STRAND_PLUS;
			} else if (strcmp(strandText, "-") == 0) {
				strand = STRAND_MINUS;
			} else if (strcmp(strandText, "+/-") == 0) {
				strand = STRAND_BOTH;
			} else if (strcmp(strandText, "*") == 0) {
				strand = STRAND_END;
			} else {
				fprintf(stderr, "Error: Unknown strand text '%s' on line %lld\n", strandText, lineNo);
				return -EINVAL;
			}

			if (strcmp(lastName, name) != 0) {
				list = nick_map_add_fragment(map, name);
				snprintf(lastName, sizeof(lastName), "%s", name);
			}
			assert(list != NULL);

			if (strand != STRAND_END) {
				nick_map_add_site(list, pos, strand);
			} else {
				list->fragment_size = pos;
			}
		}
	}
	return 0;
}

static int load_simple(gzFile file, struct nick_map *map)
{
	struct nick_list *list = NULL;
	char name[32] = "";
	double sum = 0;
	double value;
	int count = 0, i = 0, err, c;
	char buf[256];
	size_t pos = 0;

	while ((c = gzgetc(file)) != EOF) {
		if (!isspace(c)) {
			if (pos >= sizeof(buf)) {
				fprintf(stderr, "Error: Unexpected long word!\n");
				return -EINVAL;
			}
			buf[pos++] = (char)c;
		} else if (pos > 0) {
			buf[pos] = '\0';
			pos = 0;
			if (!name[0]) {
				snprintf(name, sizeof(name), "%s", buf);
				count = 0;
				i = 0;
				list = nick_map_add_fragment(map, name);
				if (!list) {
					return -ENOMEM;
				}
			} else if (count == 0) {
				count = atoi(buf);
				i = 0;
				if (count <= 0) {
					name[0] = '\0';
					count = 0;
				}
			} else {
				assert(list != NULL);
				value = atof(buf);
				sum += value;
				++i;
				if (i < count) {
					if ((err = nick_map_add_site(list,
							to_integer(sum), STRAND_UNKNOWN)) != 0) {
						return err;
					}
				} else {
					list->fragment_size = to_integer(sum);
					sum = 0;
					name[0] = '\0';
					count = 0;
					i = 0;
				}
			}
		}
	}
	if (name[0] || i < count) {
		fprintf(stderr, "Error: Unexpected EOF!\n");
		return -EINVAL;
	}
	return 0;
}

int nick_map_load(struct nick_map *map, const char *filename)
{
	long long lineNo = 0;
	char buf[256];
	int format = 0; /* 1. BNX; 2. CMAP; 3. TSV */
	gzFile file;
	int ret = 0;
	int c;

	if (strcmp(filename, "-") == 0 || strcmp(filename, "stdin") == 0) {
		file = gzdopen(0, "r"); /* stdin */
	} else {
		file = gzopen(filename, "r");
	}
	if (!file) {
		fprintf(stderr, "Error: Can not open file '%s'\n", filename);
		return 1;
	}

	c = gzgetc(file);
	gzungetc(c, file);
	if (c != '#') {  /* no any comment line, try as the simple format */
		ret = load_simple(file, map);
	} else {
		while (!gzeof(file)) {
			++lineNo;
			if (!gzgets(file, buf, sizeof(buf))) break;
			if (buf[0] != '#') break;
			if (string_begins_as(buf, "# BNX File Version:")) {
				format = 1;
				break;
			} else if (string_begins_as(buf, "# CMAP File Version:")) {
				format = 2;
				break;
			} else if (string_begins_as(buf, "##fileformat=MAPv0.1")) {
				format = 3;
				break;
			}
			if (skip_to_next_line(file, buf, sizeof(buf))) break;
		}
		if (format == 1) {
			ret = load_bnx(file, lineNo, map);
		} else if (format == 2) {
			ret = load_cmap(file, lineNo, map);
		} else if (format == 3) {
			ret = load_tsv(file, lineNo, map);
		} else {
			fprintf(stderr, "Error: Unknown input map format!\n");
			ret = 1;
		}
	}
	if (ret) {
		nick_map_free(map);
	}
	gzclose(file);
	return ret;
}

static void write_command_line(gzFile file)
{
	char name[64] = "";
	snprintf(name, sizeof(name), "/proc/%d/cmdline", getpid());
	FILE *fp = fopen(name, "r");
	if (fp) {
		gzprintf(file, "##commandline=");
		while (!feof(fp)) {
			int c = fgetc(fp);
			if (c == EOF) break;
			gzprintf(file, "%c", (c ? c : ' '));
		}
		gzprintf(file, "\n");
		fclose(fp);
	}
}

static int save_as_txt(gzFile file, const struct nick_map *map)
{
	size_t i, j;
	for (i = 0; i < map->size; ++i) {
		const struct nick_list *p = &map->data[i];
		gzprintf(file, "%s %d", p->fragment_name, p->size);
		for (j = 0; j < p->size; ++j) {
			gzprintf(file, " %d", p->data[j].pos - (j == 0 ? 0 : p->data[j - 1].pos));
		}
		gzprintf(file, "\n");
	}
	return 0;
}

static int save_as_tsv(gzFile file, const struct nick_map *map)
{
	static const char * const STRAND[] = { "?", "+", "-", "+/-", "*" };
	size_t i, j;

	gzprintf(file, "##fileformat=MAPv0.1\n");
	if (map->enzyme[0] && map->rec_seq[0]) {
		gzprintf(file, "##enzyme=%s/%s\n", map->enzyme, map->rec_seq);
	}
	gzprintf(file, "##program=bntools\n");
	gzprintf(file, "##programversion="VERSION"\n");
	write_command_line(file);
	gzprintf(file, "#name\tpos\tstrand\tsize\n");

	for (i = 0; i < map->size; ++i) {
		const struct nick_list *p = &map->data[i];
		for (j = 0; j < p->size; ++j) {
			const struct nick *q = &p->data[j];
			gzprintf(file, "%s\t%d\t%s\t%d\n",
					p->fragment_name, q->pos, STRAND[q->strand],
					q->pos - (j == 0 ? 0 : p->data[j - 1].pos));
		}
		gzprintf(file, "%s\t%d\t*\t%d\n", p->fragment_name, p->fragment_size,
				p->fragment_size - (p->size == 0 ? 0 : p->data[p->size - 1].pos));
	}
	return 0;
}

static int save_as_bnx(gzFile file, const struct nick_map *map)
{
	size_t i, j;

	gzprintf(file, "# BNX File Version: 0.1\n");
	gzprintf(file, "# Label Channels: 1\n");
	if (map->enzyme[0] && map->rec_seq[0]) {
		gzprintf(file, "# Nickase Recognition Site 1: %s/%s\n", map->enzyme, map->rec_seq);
	} else {
		gzprintf(file, "# Nickase Recognition Site 1: unknown\n");
	}
	gzprintf(file, "# Number of Nanomaps: %zd\n", map->size);
	gzprintf(file, "#0h\tLabel Channel\tMapID\tLength\n");
	gzprintf(file, "#0f\tint\tint\tfloat\n");
	gzprintf(file, "#1h\tLabel Channel\tLabelPositions[N]\n");
	gzprintf(file, "#1f\tint\tfloat\n");

	for (i = 0; i < map->size; ++i) {
		const struct nick_list *p = &map->data[i];
		gzprintf(file, "0\t%s\t%zd\n1", p->fragment_name, p->fragment_size);
		for (j = 0; j < p->size; ++j) {
			gzprintf(file, "\t%d", p->data[j].pos);
		}
		gzprintf(file, "\n");
	}
	return 0;
}

static void write_cmap_line(gzFile file, const char *cmap_id, int contig_length,
		size_t num_sites, size_t site_id, int label_channel, int position,
		int stddev, int coverage, int occurance)
{
	gzprintf(file, "%s\t%d\t%zd\t%zd\t%d\t%d\t%d\t%d\t%d\n",
			cmap_id, contig_length, num_sites, site_id,
			label_channel, position, stddev, coverage, occurance);
}

static int save_as_cmap(gzFile file, const struct nick_map *map)
{
	size_t i, j;

	gzprintf(file, "# CMAP File Version:  0.1\n");
	gzprintf(file, "# Label Channels:  1\n");
	if (map->enzyme[0] && map->rec_seq[0]) {
		gzprintf(file, "# Nickase Recognition Site 1:  %s/%s\n", map->enzyme, map->rec_seq);
	} else {
		gzprintf(file, "# Nickase Recognition Site 1:  unknown\n");
	}
	gzprintf(file, "# Number of Consensus Nanomaps:    %zd\n", map->size);
	gzprintf(file, "#h CMapId\tContigLength\tNumSites\tSiteID"
			"\tLabelChannel\tPosition\tStdDev\tCoverage\tOccurrence\n");
	gzprintf(file, "#f int\tfloat\tint\tint\tint\tfloat\tfloat\tint\tint\n");

	for (i = 0; i < map->size; ++i) {
		const struct nick_list *p = &map->data[i];
		for (j = 0; j < p->size; ++j) {
			write_cmap_line(file, p->fragment_name, p->fragment_size,
					p->size, j, 1, p->data[j].pos, 0, 0, 0);
		}
		write_cmap_line(file, p->fragment_name, p->fragment_size,
				p->size, p->size + 1, 0, p->fragment_size, 0, 1, 1);
	}
	return 0;
}

int nick_map_save(const struct nick_map *map, const char *filename, int format)
{
	gzFile file;
	int ret;

	if (strcmp(filename, "-") == 0 || strcmp(filename, "stdout") == 0) {
		file = gzdopen(1, "wT"); /* stdout, without compression */
	} else {
		size_t len = strlen(filename);
		if (len > 3 && strcmp(filename + len - 3, ".gz") == 0) {
			file = gzopen(filename, "wx"); /* 'x' is for checking existance */
		} else {
			file = gzopen(filename, "wxT"); /* without compression */
		}
	}
	if (!file) {
		if (errno == EEXIST) {
			fprintf(stderr, "Error: Output file '%s' has already existed!\n", filename);
		} else {
			fprintf(stderr, "Error: Can not open output file '%s'\n", filename);
		}
		return 1;
	}

	switch (format) {
	case FORMAT_TXT: ret = save_as_txt(file, map); break;
	case FORMAT_TSV: ret = save_as_tsv(file, map); break;
	case FORMAT_BNX: ret = save_as_bnx(file, map); break;
	case FORMAT_CMAP: ret = save_as_cmap(file, map); break;
	default: assert(0); ret = -EINVAL; break;
	}
	gzclose(file);

	if (ret) {
		unlink(filename);
	}
	return ret;
}