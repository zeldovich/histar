/*	$Id: sw-full-ls.c,v 1.15 2008/06/06 18:24:09 rumble Exp $	*/

#include <inttypes.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <inc/syscall.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

//#define MAX(_a, _b) (((_a) > (_b)) ? (_a) : (_b))

#define EXTRACT(_genome, _i) (((_genome)[(_i) / 8] >> (4 * ((_i) % 8))) & 0xf)
#define BPTO32BW(_x) (((_x) + 7) / 8)

void		bitfield_prepend(uint32_t *, uint32_t, uint32_t);
void		bitfield_insert(uint32_t *, uint32_t, uint32_t);
void		bitfield_append(uint32_t *, uint32_t, uint32_t);

#define BASE_0		0
#define BASE_1		1
#define BASE_2		2
#define BASE_3		3
#define BASE_A		0		/* Adenine */
#define BASE_C		1		/* Cytosine */
#define BASE_G		2		/* Guanine */
#define BASE_T		3		/* Thymine */
#define BASE_U		4		/* Uracil */
#define BASE_M		5		/* A or C */
#define BASE_R		6		/* A or G (Purine) */
#define BASE_W		7		/* A or T */
#define BASE_S		8		/* C or G */
#define BASE_Y		9		/* C or T (Pyrimidine) */
#define BASE_K		10		/* G or T */
#define BASE_V		11		/* A or C or G (not T) */
#define BASE_H		12		/* A or C or T (not G) */
#define BASE_D		13		/* A or G or T (not C) */
#define BASE_B		14		/* C or G or T (not A) */
#define BASE_X		15		/* G or A or T or C (any base) */
#define BASE_N		15		/* G or A or T or C (any base) */
#define BASE_LS_MIN	BASE_A
#define BASE_LS_MAX	BASE_B
#define BASE_CS_MIN	BASE_0
#define BASE_CS_MAX	BASE_3

/*
 * Give BASE_x, return the appropriate character.
 *
 * NB: Since we're limited to 4-bits, BASE_X returns 'N'.
 */
static char
base_translate(int base, bool use_colours)
{
	/*
	 * NB: colour-space only valid for 0-3 and BASE_N/BASE_X
	 *     BASE_N is reported as a skipped cycle: '.' in CS.
	 */
	char cstrans[] = { '0', '1', '2', '3', '!', '@', '#', '$',
			   '%', '^', '&', '*', '?', '~', ';', '.' };
	char lstrans[] = { 'A', 'C', 'G', 'T', 'U', 'M', 'R', 'W',
			   'S', 'Y', 'K', 'V', 'H', 'D', 'B', 'N' };

	if (use_colours) {
		assert((base >= BASE_CS_MIN && base <= BASE_CS_MAX) ||
		    (base == BASE_N || base == BASE_X));
		return (cstrans[base]);
	} else {
		assert((base >= BASE_LS_MIN && base <= BASE_LS_MAX) ||
		    (base == BASE_N || base == BASE_X));
		return (lstrans[base]);
	}
}

/*
 * Prepend the low 4 bits of 'val' to the start of the bitfield in 'bf'.
 * 'entries' is the maximum number of 4-bit words to be stored in the
 * bitfield.
 */
void
bitfield_prepend(uint32_t *bf, uint32_t entries, uint32_t val)
{
	uint32_t tmp;
	u_int i;

	for (i = 0; i < BPTO32BW(entries); i++) {
		tmp = bf[i] >> 28;
		bf[i] <<= 4;
		bf[i] |= val;
		val = tmp;
	}

	bf[i - 1] &= (0xffffffff >> (32 - (4 * (entries % 8))));
}

/*
 * Insert the low 4 bits of 'val' into the bitfield in 'bf' at
 * 'index', where 'index' is count at 4-bit fields.
 */
void
bitfield_insert(uint32_t *bf, uint32_t idx, uint32_t val)
{

	bitfield_append(bf, idx, val);
}

/*
 * Append the low 4 bits of 'val' to the end of the bitfield in 'bf'.
 * 'entries' is the number of 4-bit words in 'bf' prior to the append.
 */
void
bitfield_append(uint32_t *bf, uint32_t entries, uint32_t val)
{
	uint32_t word;

	word = bf[entries / 8];
	word &= ~(0xf << (4 * (entries % 8)));
	word |= ((val & 0xf) << (4 * (entries % 8)));
	bf[entries / 8] = word;
}


struct sw_full_results {
	/* Common fields */
	int read_start;				/* read index of map */
	int rmapped;				/* read mapped length */
	int genome_start;			/* genome index of map (abs) */
	int gmapped;				/* genome mapped len */
	int matches;				/* # of matches */
	int mismatches;				/* # of substitutions */
	int insertions;				/* # of insertions */
	int deletions;				/* # of deletions */
	int score;				/* final SW score */

	char *dbalign;				/* genome align string */
	char *qralign;				/* read align string */

	/* Colour space fields */
	int crossovers;				/* # of mat. xovers */
};

struct swcell {
	int	score_north;
	int	score_west;
	int	score_northwest;

	int8_t	back_north;
	int8_t	back_west;
	int8_t	back_northwest;
};

#define FROM_NORTH_NORTH		0x1
#define FROM_NORTH_NORTHWEST		0x2
#define	FROM_WEST_NORTHWEST		0x3
#define	FROM_WEST_WEST			0x4
#define FROM_NORTHWEST_NORTH		0x5
#define FROM_NORTHWEST_NORTHWEST	0x6
#define FROM_NORTHWEST_WEST		0x7

#define BACK_INSERTION			0x1
#define BACK_DELETION			0x2
#define BACK_MATCH_MISMATCH		0x3

static int		initialised;
static int8_t	       *db, *qr;
static int		dblen, qrlen;
static int		a_gap_open, a_gap_ext;
static int		b_gap_open, b_gap_ext;
static int		match, mismatch;
static struct swcell   *swmatrix;
static int8_t	       *backtrace;
static char	       *dbalign, *qralign;

/* statistics */
static uint64_t		swticks, swcells, swinvocs;

static int
full_sw(int lena, int lenb, int threshscore, int maxscore, int *iret, int *jret)
{
	int i, j, max_i, max_j, max_score;
	int sw_band, ne_band;
	int score, ms, a_go, a_ge, b_go, b_ge, tmp;
	int8_t tmp2;

	max_i = max_j = max_score = 0;

	/* shut up gcc */
	j = 0;

	score = 0;
	a_go = a_gap_open;
	a_ge = a_gap_ext;
	b_go = b_gap_open;
	b_ge = b_gap_ext;

	for (i = 0; i < lena + 1; i++) {
		int idx = i;

		swmatrix[idx].score_northwest = 0;
		swmatrix[idx].score_north = 0;
		swmatrix[idx].score_west = 0;

		swmatrix[idx].back_northwest = 0;
		swmatrix[idx].back_north = 0;
		swmatrix[idx].back_west = 0;
	}

	for (i = 0; i < lenb + 1; i++) {
		int idx = i * (lena + 1);

		swmatrix[idx].score_northwest = 0;
		swmatrix[idx].score_north = -a_go;
		swmatrix[idx].score_west = -a_go;

		swmatrix[idx].back_northwest = 0;
		swmatrix[idx].back_north = 0;
		swmatrix[idx].back_west = 0;
	}

	/*
	 * Figure out our band.
	 *   We can actually skip computation of a significant number of
	 *   cells, which could never be part of an alignment corresponding
	 *   to our threshhold score.
	 */
	sw_band = ((lenb * match - threshscore + match - 1) / match) + 1;
	ne_band = lena - (lenb - sw_band);

	for (i = 0; i < lenb; i++) {
		for (j = 0; j < lena; j++) {
                        struct swcell *cell_nw, *cell_n, *cell_w, *cell_cur;

                        cell_nw  = &swmatrix[i * (lena + 1) + j];
                        cell_n   = cell_nw + 1; 
                        cell_w   = cell_nw + (lena + 1);
                        cell_cur = cell_w + 1;

			/* banding */
			if (i >= sw_band + j) {
				memset(cell_cur, 0, sizeof(*cell_cur));
				continue;
			}
			if (j >= ne_band + i) {
				memset(cell_cur, 0, sizeof(*cell_cur));
				break;
			}

			/*
			 * northwest
			 */
			ms = (db[j] == qr[i]) ? match : mismatch;

			tmp  = cell_nw->score_northwest + ms;
			tmp2 = FROM_NORTHWEST_NORTHWEST;

			if (cell_nw->score_north + ms > tmp) {
				tmp  = cell_nw->score_north + ms;
				tmp2 = FROM_NORTHWEST_NORTH;
			}

			if (cell_nw->score_west + ms > tmp) {
				tmp  = cell_nw->score_west + ms;
				tmp2 = FROM_NORTHWEST_WEST;
			}

			if (tmp <= 0)
				tmp = tmp2 = 0;

			cell_cur->score_northwest = tmp;
			cell_cur->back_northwest  = tmp2;


			/*
			 * north
			 */
			tmp  = cell_n->score_northwest - b_go - b_ge;
			tmp2 = FROM_NORTH_NORTHWEST;

			if (cell_n->score_north - b_ge > tmp) {
				tmp  = cell_n->score_north - b_ge;
				tmp2 = FROM_NORTH_NORTH;
			}

			if (tmp <= 0)
				tmp = tmp2 = 0;
				
			cell_cur->score_north = tmp;
			cell_cur->back_north  = tmp2;

			
			/*
			 * west
			 */
			tmp  = cell_w->score_northwest - a_go - a_ge;
			tmp2 = FROM_WEST_NORTHWEST;

			if (cell_w->score_west - a_ge > tmp) {
				tmp  = cell_w->score_west - a_ge;
				tmp2 = FROM_WEST_WEST;
			}

			if (tmp <= 0)
				tmp = tmp2 = 0;

			cell_cur->score_west = tmp;
			cell_cur->back_west  = tmp2;


			/*
			 * max score
			 */
			if (cell_cur->score_northwest > score ||
			    cell_cur->score_north > score ||
			    cell_cur->score_west > score)
				max_i = i, max_j = j;

			score = MAX(score, cell_cur->score_northwest);
			score = MAX(score, cell_cur->score_north);
			score = MAX(score, cell_cur->score_west);

//			if (score == maxscore)
//				break;
		}

//		if (score == maxscore)
//			break;
	}

	*iret = max_i;
	*jret = max_j;

	return (score);
}

/*
 * Fill in the backtrace in order to do a pretty printout.
 *
 * Returns the beginning matrix cell (i, j) in 'sfr->read_start' and
 * 'sfr->genome_start'.
 *
 * The return value is the first valid offset in the backtrace buffer.
 */
static int
do_backtrace(int lena, int i, int j, struct sw_full_results *sfr)
{
	struct swcell *cell;
	int k, from, fromscore;

	cell = &swmatrix[(i + 1) * (lena + 1) + j + 1];

	from = cell->back_northwest;
	fromscore = cell->score_northwest;
	if (cell->score_west > fromscore) {
		from = cell->back_west;
		fromscore = cell->score_west;
	}
	if (cell->score_north > fromscore)
		from = cell->back_north;

	assert(from != 0);

	/* fill out the backtrace */
	k = (dblen + qrlen) - 1;
	while (i >= 0 && j >= 0) {
		assert(k >= 0);

		cell = NULL;

		/* common operations first */
		switch (from) {
		case FROM_NORTH_NORTH:
		case FROM_NORTH_NORTHWEST:
			backtrace[k] = BACK_DELETION;
			sfr->deletions++;
			sfr->read_start = i--;
			break;

		case FROM_WEST_WEST:
		case FROM_WEST_NORTHWEST:
			backtrace[k] = BACK_INSERTION;
			sfr->insertions++;
			sfr->genome_start = j--;
			break;

		case FROM_NORTHWEST_NORTH:
		case FROM_NORTHWEST_NORTHWEST:
		case FROM_NORTHWEST_WEST:
			backtrace[k] = BACK_MATCH_MISMATCH;
			if (db[j] == qr[i])
				sfr->matches++;
			else
				sfr->mismatches++;
			sfr->read_start = i--;
			sfr->genome_start = j--;
			break;

		default:
			fprintf(stderr, "INTERNAL ERROR: from = %d\n", from);
			assert(0);
		}

		/* continue backtrace (nb: i and j have already been changed) */
		cell = &swmatrix[(i + 1) * (lena + 1) + j + 1];

		switch (from) {
		case FROM_NORTH_NORTH:
			from = cell->back_north;
			break;

		case FROM_NORTH_NORTHWEST:
			from = cell->back_northwest;
			break;

		case FROM_WEST_WEST:
			from = cell->back_west;
			break;

		case FROM_WEST_NORTHWEST:
			from = cell->back_northwest;
			break;

		case FROM_NORTHWEST_NORTH:
			from = cell->back_north;
			break;

		case FROM_NORTHWEST_NORTHWEST:
			from = cell->back_northwest;
			break;

		case FROM_NORTHWEST_WEST:
			from = cell->back_west;
			break;

		default:
			fprintf(stderr, "INTERNAL ERROR: from = %d\n", from);
			assert(0);
		}

		k--;

		if (from == 0)
			break;
	}

	return (k + 1);
}

/*
 * Pretty print our alignment of 'db' and 'qr' in 'dbalign' and 'qralign'.
 *
 * i, j represent the beginning cell in the matrix.
 * k is the first valid offset in the backtrace buffer.
 */
static void
pretty_print(int i, int j, int k)
{
	char *d, *q;
	int l, done;

	d = dbalign;
	q = qralign;

	done = 0;
	for (l = k; l < (dblen + qrlen); l++) {
		switch (backtrace[l]) {
		case BACK_DELETION:
			*d++ = '-';
			*q++ = base_translate(qr[i++], false);
			break;

		case BACK_INSERTION:
			*d++ = base_translate(db[j++], false);
			*q++ = '-';
			break;

		case BACK_MATCH_MISMATCH:
			*d++ = base_translate(db[j++], false);
			*q++ = base_translate(qr[i++], false);
			break;

		default:
			done = 1;
		}
		
		if (done)
			break;
	}

	*d = *q = '\0';
}

static int
sw_full_ls_setup(int _dblen, int _qrlen, int _a_gap_open, int _a_gap_ext,
    int _b_gap_open, int _b_gap_ext, int _match, int _mismatch,
    bool reset_stats)
{

	dblen = _dblen;
	db = (int8_t *)malloc(dblen * sizeof(db[0]));
	if (db == NULL)
		return (1);

	qrlen = _qrlen;
	qr = (int8_t *)malloc(qrlen * sizeof(qr[0]));
	if (qr == NULL)
		return (1);

	swmatrix = (struct swcell *)malloc((dblen + 1) * (qrlen + 1) * sizeof(swmatrix[0]));
	if (swmatrix == NULL)
		return (1);

	backtrace = (int8_t *)malloc((dblen + qrlen) * sizeof(backtrace[0]));
	if (backtrace == NULL)
		return (1);

	dbalign = (char *)malloc((dblen + qrlen + 1) * sizeof(dbalign[0]));
	if (dbalign == NULL)
		return (1);

	qralign = (char *)malloc((dblen + qrlen + 1) * sizeof(dbalign[0]));
	if (qralign == NULL)
		return (1);

	a_gap_open = -(_a_gap_open);
	a_gap_ext = -(_a_gap_ext);
	b_gap_open = -(_b_gap_open);
	b_gap_ext = -(_b_gap_ext);
	match = _match;
	mismatch = _mismatch;

	if (reset_stats)
		swticks = swcells = swinvocs = 0;

	initialised = 1;

	return (0);
}

#define cpuhz()	1.0
#if 0
static void
sw_full_ls_stats(uint64_t *invoc, uint64_t *cells, uint64_t *ticks,
    double *cellspersec)
{
	
	if (invoc != NULL)
		*invoc = swinvocs;
	if (cells != NULL)
		*cells = swcells;
	if (ticks != NULL)
		*ticks = swticks;
	if (cellspersec != NULL) {
		*cellspersec = (double)swcells / ((double)swticks / cpuhz());
		if (isnan(*cellspersec))
			*cellspersec = 0;
	}
}
#endif

static void
sw_full_ls(uint32_t *genome, int goff, int glen, uint32_t *theread, int rlen,
    int threshscore, int maxscore, struct sw_full_results *sfr)
{
	struct sw_full_results scratch;
	uint64_t before;
	int i, j, k;

	before = 0;	//XXXXX

	if (!initialised)
		abort();

	swinvocs++;

	assert(glen > 0 && glen <= dblen);
	assert(rlen > 0 && rlen <= qrlen);

	if (sfr == NULL)
		sfr = &scratch;

	memset(sfr, 0, sizeof(*sfr));
	memset(backtrace, 0, (dblen + qrlen) * sizeof(backtrace[0]));

	dbalign[0] = qralign[0] = '\0';

	for (i = 0; i < glen; i++)
		db[i] = (int8_t)EXTRACT(genome, goff + i);

	for (i = 0; i < rlen; i++)
		qr[i] = (int8_t)EXTRACT(theread, i);

	sfr->score = full_sw(glen, rlen, threshscore, maxscore, &i, &j);
	k = do_backtrace(glen, i, j, sfr);
	pretty_print(sfr->read_start, sfr->genome_start, k);
	sfr->gmapped = j - sfr->genome_start + 1;
	sfr->genome_start += goff;
	sfr->rmapped = i - sfr->read_start + 1;
	sfr->dbalign = strdup(dbalign);
	sfr->qralign = strdup(qralign);

	swcells += (glen * rlen);
	swticks += (0 - before);	//XXXXX
}

int
main()
{
	const char *s_genome = "GACCTTGACCTGTCCAGGTCTTTTTTTCACCTTTTTCGTCAATATATTGATATTTCGCAGGGCGAATGGCGCGTTTTGTTTTAGCTGTTACTTTATTAGAAGTACTTATTTGCAATAACTCGTTAGGGTCAATACCCTCTTCAACTAAAATTGCCTTCATTTTTTCTAATTTACGTTTATATTCTTCTTTATCCTTTTTAGCCTTATTTTCTTCTTGTTTTCGGTCATTCACTACAGCCATCAATTTTTCAAGCATTTCTTCTAATGTTTCAAGACTACATTCACGGGCTTGAGCACGCAACGTACGTATATTATTCAGAATTTTTAAAGCTTCACTCATTGTTATAATCTCATATTTAAATAATATTAGAGGGTGTGTAAATAATAATAGAATGCTATTTTCTTTTCTGCAATAGCGAACTTTAAGTCAGTTCAAAAGTTTTACGCTTCTTAAAGCTAAAATTTTTTCAAATTCACTTTCAATTTAGATAAATTTTTACTTTTTGATAAAATGTTGTTTTATAAAAACATGATTTTATTATTAAATAATAAGTTTCCTTTATATTAATATCTTGCTTTAAAGTTAATTTTTCTTTTTTAATATGATTAAGAAAAAAATTGTCTCAAATTTGTTAAATTAAATTAACTTAACTAAAAATCTACTGTCGAGCTTTTTTAATAAAAGACAAACTTAATTTTTTTTAAGTAGTTAAATTAATCGCATAAATTAATATGACTCATAACAAAAATAAACAAGTGATTTTTTATAAATACTTTTGATCCTGATCAGCAGGTTGATTTTTTATGTATTTTATCAATCTTATTATTTTTATAACAAGGGGCTAAAAAAATAAAACAATCGCTCAATGATGCTCTGCCAGACAGATCGTTTTTCCCACTGATCTTTTTCTAAAAGAACAGAATTTTTCACATAATCATTTTGCAGACTGGCCAGATTTTCTCCAAAATCACGATCATCTATGGCAACAGTAATTTCAAAATTAAGCCATAAACTTCGCATATCGAGATTCACGGTTCCAATTAAGCTCAGCTGTTTATCTACCAAAACACTTTTACTGTGTAGCATTCCTCCTTGAAATTGATAAATTTTAACTTTGGCTTTTAATAAGTCAGCAAAAAAAGCACGGCTTGCCCAACTCACTATCATAGAATTGCTACGATAGGGAAGAATAATGATCACCTCTACTCCCCTTTGAGAGGCCGTACAAATGGCATGAAGCAAGTCTTCACTCGGGACAAAATAAGGCGTCGTCATAATCAGTTGTTCACGTGCAGAATACACTGCCGTTAACAGGGCTTGATGGATAATGTCTTCAGGAAATCCTGGCCCAGAGGCAATCACCTGAAGAGGGCCTGCTGTTTTATCATGCTCTTTAAGATCAATATTGTGACGGGAAAATTCAGGTAAAATGTGTTTACTGGTTTCCATTTGCCAGTCGCAAGCATAAACCATGCCGATGGCGGTCGCTATAGAACCCTCCATTCGAGCCATCAGATCAACCCATGGGCCCACCTTGTTTTTTTTCTTGGTATGATGAGAAGTATCGACCATATTCATACTGCCTGTGTACACAATGTAGTTATCAATCAGCACAATTTTGCGATGTTGTCTCAAATCCATACGACGAAAAAACATGCGTAATACATTCACTTTTAAAGCTTCGACAAGATGAATACCCGCATTCAACATCATTGCGGGATACTGGCTACGAAAAAAAGCTCGGCTTCCTGCAGAATCGAGCATGAGACGGCAGTTGACACCTCGACGTGCTGCCGCCATTAATGCTTCGGCAATGTCATCTACCCATCCCCCTACTTGCCAGATATAAAAAACCATCTTTATATCATGTCGAGCCTGTTGAATATCTCTTATCAATCTTTTTAACGTTTGCTCATTAGTAGTGAGGAGCTCTAATTGATGCGCCTTCATCGCGCCGATTCCTTGACGAAGTTCACAGAGCTGAAATAAAGGCGCTGCGACAGCACTATGTGGAGGAAAAAAGAGCTGTCTGGCGTCTTTTGATTGATTTAATGCGTTTGTTGTTAATGGCCATAAAGCTTTTGCTCTCAGCGCCCGAGCCTTACCTAAATTAAGTTCACCAAAAGAGAGATAAGCGATAATGCCGACTAAAGGAAAAATATAGATCACCAATAACCAAGCCATAGCAGAGGGAACGCCTCGTCGTTTAATTAAAATGCGTAATGTGATCCCTGCAATCAGTAGCCAATATCCAAATAACACAATCGAGCTGATAAAGGTGTAAAATGTGTTCATAAATAAATTACAATCCTTTTGTTTTGTGCATTTATCAAAATAAGTGAGTAAATTAATGACACAAATAATATTTTAAACATCATTGTCGGTATTTCATCTGCAAATGCTGAATTAGCATATGCTTTATCCGTCGGTTACGCTTCGAATTAATGTGGAATTAATGAAAAAGAGTTCAACCTTTAATTTAAGAAAAATATTAACTCTGAAGGGGTTAACGTTAAATATCGTTGTCGATTTTTTTATGAATAAACTAAAATGTTTAATGGCAATAAAAAAGGTGTGACGGCAATTTTTTAGGCATTCGTTTTTAATTAAGCCAATTTTTTATCATACTATCTATTCTATAAAGTTACCGGATTGAAAATTCTTGCGTAAAAAATCTTAAAACCAAAATTGCCAGATTTTTTTATTCAAAATAAGTTAATTTAAGAAAAAGAAAGTCAAATAAAACCTGAGATAAAAGTTTTGAAAATTTTATCATGTACAGCTATGCGATTTATCTTATATAATATCGAAGAGACAATCTTAACTCTGTTTTTTATTTTTGTCTTTTTATCTATCTTTAGCAATAAAAATATTTTTATACAAAATAAATAGGGTGTGCTATGAAAAAAACTTTAATATACAGTGTATTAGCAATGGGCTCAATGGTCGGCATTTCATCTGCAAATGCTGAATTAGAAAATTCTTTATCCGTAGGTTACGCTTGGAGTTCATTGGGGGTTATCGACGAGAAAAATAAGTTAAAAACAAATAAAGATGCTGGCCCTAAAGGGATTAACCTTAAATATCGTTATGAGATAGATAACCAGCTAGGGATCATCACTTCTTTCACTTATTTAAGAAAAAAAGACGACCTATTGGTGAAAATTGGGTCAACAGATATAAATTATTCATTAAATTTTAATTATTATTCTCTTTTAATAGGGCCAACTTATCGCTTTAATGAATGGGTGAGCGGCTATGCTGCATTAGGTTTCGTTCATTCAAAAGTAAAAGAAGCAGCTAAGTTCATAAATATTGATGAAAAAGCTGATAACAAGAAAAACGGCTTTGCAGGTATGATAGGTTTTCAGGTGAATCCAATGACAAATATTGTTATCGATGGTTCTTTTGAATATGCTAAATCTGTCTATGGAAAAAAAGCGAAGGTTTGGTCAATTGGCTTAGGCTATCGCTTTTAATTAAGCAAAACCATATTATTTATGAGCCACCGGAAGGTGGCTTTTTTTATCATTTGTTAAAAATTTTCCATAAAGAATGTTAAAACAAAATTTGTGGGCGCCTTTTTTAAAAGCTTGGCCTTTTTTATTTTCTTTAGGGTTGCATGTTTTTTTGATCTTTATGCTTTTGCATTTTTTTATTGAGATCCCACCCCCAAATTTAGCGTCTTCGTCTTTAAAAGTGCACATGAACTATCATGGAGCTTCTTTTTTAAAGCCCTCTTTTATCGAACAGCCCTCTCAAAAGTTTACAACACCAGCGGATCTTGTTGATTCTGCTGAACTGCGCTCATTACCGATTAAAAAAAATAAATATAGCCCGATAAAAAAAACAAAAAAACATCCCTCTAAAAAATCTGCCCCAAACAAAATGCCGGTGATAAAAAACGAAGCCTCTTCCGATCCCGATTTTTTGACTCAATCAATTCAAAAAAGAGAAGGTGATGTTGTTGAATCCAGCCAAACGGCTTCCTCTTCTACAGGAGCAGATACAGAGCCGGTATCGTTTAAAAAAGTGAAACCTGTTTATCCTGCACGAGCTTTGGCTCTCGGGATTGAGGGAAAAGTGAAGGTGCAATATGACATTGATGATCAAGGCAGGGTAAAAAATATACGTCTACTAGAATCTGATCCCCCTCATATTTTTGAACGTAGCGTCAAAACGGCGATGAAAAAGTGGAAATTTCAAGCGCATCCTTTTAAAAACCGTGTGACAGTGATTATTTTTAAGCTCGACAATAATCAAATGCCAGTCGAATAAAAGGGAAGATGTTCTAAAACTTTCAAACCAATTTTCAGTGGCTTTTTTCATGTCATTTTTTTTTATCAGTTTGATCGAGGTGCCGATAAATATAAACAACCGTCAACAAAGTGAATAAAAAAGTGGCCGCGGTCAAACCAAATACTTTGAAATTGACCCAAATTGATAGCGGAAACCAAAATGCAATATAAATATTAGTGATACCACAGATTAAAAAAAAGAGCGCCCAATACAAGTTCATTCTTGACCAAATGTGATCAGGTAAACTCAATTCTTTACCCAAAAGGCGTTGTATTAACGTTTTTTTAAATATCCACTGGCCGATAAGTAAAGCCGCAGAGAATAAAAAATAAATGATGGTCACTTTCCACTTAATAAAAATATCACTGTGAAAAATTAATGTCATGCTGCTCAATACCACGACAATAATTGTACTCACCAGCATCATGGTTTCTATTTTTCGATATTTTATCCAAACCAGAGCCAAAACGAAGAGACTGGAAACAATCAATGCACCGGAAGCAACAAAGATGTCATATTTTTTATAAAAAATAAAAAATATGATTAACGGGATCAAATCTAAAAGTGTTTTCATTTTATGCCCGCAATAACATGTATAAACGAAATAAATAAACCAGTAAGAATGCCAATAAAAAATAATTTAAGGCGTTAAATATCAGTATCATCATATTGTGAGAAGAAATATTAAGGTGATTGATTGAAAGCAACAGCAAAAGCAGGAGTTTGGCGCTCCACCAGAGCAACATAGCTGGAATAATGAGACGAGCATGAGTAAATGCCATTTTACTGCTGCGGCTTATGGCCGAAAAAATCCCTACTTTTTCATTGATAAAAATAATAGGAGAGAACGAAACGCTCAGTGCGATAATAATGCCAGGAATAATAAAAGCGGTTAGTCCAAGTTGGATCAATAGTGTACAAATAAAAACCAATAAAAATAACTTAGGCAGTACAGGTAAGAAAGTAGACAGAGTTTTCAAAAATCTGACAGGGCGATTGTTTGATATCTCTATCATGATACTTAACATCCCTCCTATTAAAAGAACATGACCCAATAGCATTGAAAAAGTCGCGCAAGCGGATATTTTCAGTATTGCCATTTGCTGCTCAGGGCTCATCTGAGAGATCATCTGTTTAATCCCTATATTTTGAGCGTCAATCTGGTTTTTTGTGGTTTCTAAAATGTTCAGTTGTTCCTGGGTGGGTAAAAATAAATGATTAATCATGACGGTTGTTAATGCAGCGAATAATGTCAGCAATACAATCGTGAATAATTCATTTCGCAAAAAATTAAAACTGTCACGGTATAAGCGATTTGCCGTGATAGACATAGAGGCTCCTAAAAATCAACATAAAAAACGACGCATTATACTCTCGTTAAAGTACCTTGTGCGCCTTAAAAGTTCATATCATACGAATATTTTTGGATTTTTAAACAGGGTATTCACAGCTATGAGTTTTTTCGTTTGTTTTGATTGATTAAAAAAATGAATATAAAGGACTGCAGAGTAAAACCCCGGTCAATCCTTAGGTAGCCAGTATGCCTCGTGTTTACAGATGGGGGTAGAAAGAGGGTAAATAAAATGCAGTCAAGGTAACGATAGGAAATAGCTTACAAACTTGACTGTACGTTTCATGATTTGAAAGCCAAGGGTATTTCGGATTACAAAGGGAGTATAATTGATAACTATGGCGGAGGAGGTGAGATTCGAACTCACGAACGGTTGCCCGTCGACGGTTTTCAAGACCGTTGCCTTCAGCCACTCAGCCACCCCTCCAAACGCGGATCGCACTATAAACACTGTCTACAGTGTTGTAAAGTAGTAGATAATTTTATTATTTTCAATTCTCTTCATCAGGAAAATTTTATGAAAAAAGTAGGTTTGATAGGTTGGCGCGGCATGGTTGGCTCGGTATTAATGCAACGTATGAAAGAAACCGATGATTTTAAACGCTTAGACCCGATTTTTTTTTCCACAACAGAATCTCAAAAAGGTCAAGCCAATCCTGATTTTTCTCCTACAATGGGCAAAATACAAGATGCTCATCATATCGAAACATTAAGATCATTAGATATCATTCTGACCTGCCAGGGAGGGGAATATACCAACAGAGTTTATCCGAAATTGCGTGCCACAGGTTGGCAGGGTTATTGGATTGATGCCGCCTCTGCATTGCGGATGGAAAAAGACAGTGTTATAGCGCTGGATCCTGTGAATTTGAGCCTGATCGAATGTGCTTTAAATAGAGGGATTAAAACCTTTGTCGGCGGCAATTGCACCGTGAGCTTAATGCTGATGTCTCTGGGAGGATTGTTTAATGCCTCTTTGATCGAATGGGTTTCGTTTGCAACTTATCAAGCCGCTTCAGGGGCCGGGGCACGTCATATGCGTGAATTATTAACTCAAATGGGGCTTTGCTATACAGAAGTAGCGTCACAGCTACAAGATCCCGCTGCAACGATTCTCGCAATAGAGCGCGAGATCATGGCATTGACACGGAGCGGCGATCTTCCAACGGACTGTTTCGAAGCCCCTTTAGCAACAAGTTTGATTCCGTGGATTGATATCGCTTCAGAAAATGGCCAGACTCGTGAAGAATGGAAAGCGGAGGTGGAAGCCAACAAAATTTTGAACAATAAAACGATGATCCCCATTGATGGCATATGCGTACGTATCGGCGCTTTGCGTTGCCATAGCCAGGCGATTACACTCAAACTAAAAAAAGATGTTCCTATCAAAGAGATAGAACAAATTCTGGGTTCTTATCATGAATGGGTCAAGGTGATTCCAAACAAAAAACAAACGACTTTAGACAGACTCACTCCGGCTGCGGTAAGCGGTACTCTTGATGTGCCCGTGGGTCGATTACGAAAACTAAGTATGGGACCCAAATATTTATCTGCTTTTACTGTTGGCGATCAACTATTGTGGGGTGCCGCTGAGCCTCTTCGGCGTATGTTGAATTTGCTATTGTGAAAGTTAAAAAAATAGAATTTAGTTGATTTTTAAAAAGCTCTAATATCGGTATGTTTTCAATCTTTAACAGGCCTACCGTATTCGTAAACCCTTTAGGATGAAAAATGAAAGACAGCGATATTGTCAAAAAAAGGCATATGTTAGATAAACAACATATCAAACTCACTTACATCACTCATTTTTACTGCAATCAAGCCAATATCAATTCTGTAGAGTCTTTATTGAGAGAGTATGAAAACTACCCTGATGATATTAAAGACCAGGTTGAATTTGTGATTGTGGATGATTGCTCTCCATTAAAATACAAAGTAAATCATTTTGATCTCAATATCACTTGGTTAAAAATCACTGAGGATATTGAATGGAATCAAGCCGGCGCAAGAAATTTAGGGGTTGTTTATGCAAAATCAGACAAAATTATTTTGATGGATCTCGATTGTAAACTGCCGGAAGACACCTTTAGATATCTGGTCAATGCAGGAAATCCTGAGCGATCTTTTTATCGGATTTATCGAACCGATCCTCAAACAAAAAAAGCATGTAGAGGGCATCCGAATGTCTTCTTTTTATCCAGAGCAAGATTTTTCAGGCTGTACGGATACGATGAGGAATTTGCGGGTCATTATGGCGCTGAAGATTATCGTTTTGTCAAATTTCATAAAGATCATGGCTCAAAACCAAAATATCTGGCTAAAAAATTCAGGTGTATCGATCGCAATATAGATAGAGAAAAAAGTTATCATTCTTTGAACAGAGATCTTTCTCATAACACCCCCATAGATTTAAGAAAAAAGCTTGAAATCGCTCGATTCGGTAAAGAATACGGTCATAGTCGAATCTTTCTGAACTTTACATGGTCTATTGATACTATTTATTCTCGAACAGCTCCGATTCCTGTTCAAAAAAGATGGTGGAGGCCATTATGGTGGTTCCGCTATTTGTTTAGATGTTTCTCTGTGTAAAATACATTCTTTAAATTTAGGATCCTCAATGATATTTTCCGCTCCATTTTTAAATTACTTTACACCTCAACACCGCTTCACTACATTTGCAACTGTGATCGCATTCTTGTTAGGTTTTTCTTTACCTACCTCTAATGTTTTAATGCATTTTTTGCTTGTATTGGCGTTACTCTGTGTTTTTTTGAGGCCTAATCGCCATTTCATAAAAATTTTAGCTAAAAATCCTCTTGTTTGGTTACCTGCTGTCTTGTTTTTATTGTTGCTTCTCTCACTCCTGACTCAGTACGATGTCTATGGAGTCAAGATGATCTATAAATACAAAAAACTATTATTTATTTTGCCCCTTTCTCTTTTTTTTTTAATGTCTCGATCGTTGTCCAAACAGTTTGTAAAAGGATTTTTATTAGCGAATGCCATTATTTTATTTACCAGTTTTCTCATCTGGGAATTTGATCTTCCGTTGGGCACAGCCTACCTGAGCAATCCCACTGTTTTTAAGATGCATATCACCCAAAATTTTTTTATGGCCTTGGCCGTGCTGATCTGGTTGCAACAAACTTTTCATCATAAAGGAATAAAATCTGGCGGGTATGCATTACTGACTTGCCTGGGGGTTTATAATATCTTTTTCATGGTGCAAGGTAGAACAGGGTATCTTGCTTTAGTAGTTGCTTTTTTTACCTGGATTTTTTTATCCTGCTCTAAAGTGCATCAATTGAGAATGATCATAGTTGCGCTTATTTTAGGCGCTGTTCTTGTCA";
	const char *s_reads[3] = {
		"TTTGTAGTCGCTCAGATGTCATCTGAAGACAAATCAAGCCATAGAACTTGTCAGAATAGGTTTGGACTGTCTTGGTGGCTCAACCAATGACTCTGTTTCTGCCGAATCACATTCAACACCTGCCTTCTCGACAGACCGTATTTCAGGGCCAGTCGGTCGGCTCTGGAGCCGGTGAGGGTGCCGCTGTGGTATTCCTGCCGAATGTGATGATTCCGAACGGCGCGAAAATATTTGCTCGGCAGGTCTAAGACATTGCCGGCATAATGTGCACAAAGCCGCTCGGCATCCTGACCTCCCAGCAAGAGCCCCCTCGAGACTCNC",
		"GGTTTAGATACCCTCTTAAAGCGAATCACTGAATCTTTAAACACGAGTCAGTGCAAATCATGGGTTTTGGTACCTTTAAGATCAAACCTCGTAAGGCGCGTAATGCTCGTAACCCTAAGACTGGTCAACCAATGAAAATTCCTGCATGCGATGTCCCTACTTTGAGCTTTGGAAAATTCTTAAAAACGAACTTAAAGGGTCACAGAGTGTGGAGAAAAACAAGATCAGTAGCACCAGTCGGAAAGAAGC",
		"TTTTCGATATTTTATCCAAACCAGAGCCAAAACGAAGAGACTGGAAAGAATCAATGCACCGGAAGCAACAAAGATGTCATATTTTTATAAAAATAAAAATATGATTAATGGGATCAAATCTAAAAGCGTTTTCATTTTATGCCCGCAATAACATGTATAAACGAAATAAATAAACCAGTAAGAATGCCAATAAAAATAATTTAAGGCGTTAAATATCAGTATCATCATATTGTGAGAAGAATATTAAGGTG"
	};
	static uint32_t lookup[256] = { ['A'] 0, ['C'] 1, ['G'] 2, ['T'] 3, ['N'] 15 };
	uint32_t *genome, *reads[3];
	int genome_len, read_lens[3];
	int i, j, maxreadlen = 0;
	struct sw_full_results sfr;
	const int iter = 10;
	uint64_t nsec0, nsec1, total_nsec = 0;
	char *cachedestroy = NULL;

	genome_len = strlen(s_genome);
	genome = malloc(BPTO32BW(genome_len) * sizeof(uint32_t));
	for (i = 0; s_genome[i] != '\0'; i++) {
		uint32_t val = lookup[(unsigned char)s_genome[i]];
		bitfield_append(genome, i, val);
	}

	for (i = 0; i < 3; i++) {
		read_lens[i] = strlen(s_reads[i]);
		maxreadlen = MAX(read_lens[i], maxreadlen);
		reads[i] = malloc(BPTO32BW(read_lens[i]) * sizeof(uint32_t));
		for (j = 0; j < read_lens[i]; j++) {
			uint32_t val = lookup[(unsigned char)s_reads[i][j]];
			bitfield_append(reads[i], j, val);
		}
	}

#define CACHEDESTROYLEN 1024 * 1024
	cachedestroy = malloc(CACHEDESTROYLEN);

	sw_full_ls_setup(genome_len, maxreadlen, -40, -7,
	    -40, -7, 10, -10, 1);

	for (i = 0; i < iter; i++) {
		for (j = 0; j < CACHEDESTROYLEN; j++)
			cachedestroy[j] = 0xce;
		nsec0 = sys_clock_nsec();
		sw_full_ls(genome, 0, genome_len, reads[0], read_lens[0],
		    0, 999999999, &sfr);
		nsec1 = sys_clock_nsec();
		total_nsec += (nsec1 - nsec0);
	}

	printf("%" PRIu64 " nsecs\n", total_nsec);

	return (0);
}
