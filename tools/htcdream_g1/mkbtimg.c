#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

#include <sys/stat.h>

#include "bootimg.h"

static void *
xmalloc(size_t len)
{
	void *ret = malloc(len);
	if (ret == NULL) {
		perror("malloc failed");
		exit(1);
	}

	return (ret);
}

static void
xfread(void *ptr, size_t size, size_t nitems, FILE *fp)
{
	if (fread(ptr, size, nitems, fp) != nitems) {
		perror("fread failed");
		exit(1);
	}
}

static void
xfwrite(const void *ptr, size_t size, size_t nitems, FILE *fp)
{
	if (fwrite(ptr, size, nitems, fp) != nitems) {
		perror("fwrite failed");
		exit(1);	
	}
}

static void 
get_off_and_file(char *s, uint32_t *off, char **file)
{
	char *cp;

	cp = strchr(s, ':');
	if (cp == NULL)
		goto err;
	*cp = '\0';
	*file = cp + 1;
	*off = strtoul(s, NULL, 0);
	if (strlen(*file) < 1)
		goto err;

	return;

err:
	fprintf(stderr, "error: invalid parameter: [%s]\n", s);
	exit(1);
}

static int
load_section(int pgsize, char *file, char **section, uint32_t *size, uint32_t *pages)
{
	struct stat sb;
	FILE *fp;

	if (stat(file, &sb) != 0) {
		fprintf(stderr, "error: failed to stat file [%s]: %s\n",
		    file, strerror(errno));
		exit(1);	
	}
	fp = fopen(file, "r");
	if (fp == NULL) {
		fprintf(stderr, "error: failed to open file [%s]: %s\n",
		    file, strerror(errno));
		exit(1);
	}

	*size = sb.st_size;
	*pages = (sb.st_size + pgsize - 1) / pgsize;
	
	*section = xmalloc(*pages * pgsize);
	memset(*section, 0, *pages * pgsize);
	xfread(*section, *size, 1, fp); 
	fclose(fp);
}

static int
overlap(uint32_t s1, uint32_t l1, uint32_t s2, uint32_t l2)
{
	return ((s2 >= s1 && s2 < (s1 + l1)) || (s1 >= s2 && s1 < (s2 + l2))); 
}

static void
sanity_check(boot_img_hdr *hdr)
{
	if (overlap(hdr->kernel_addr, hdr->kernel_size, hdr->ramdisk_addr, hdr->ramdisk_size)) {
		fprintf(stderr, "error: kernel and ramdisk sections will overlap in memory!\n");
		exit(1);
	}
	if (hdr->second_size != 0 && overlap(hdr->kernel_addr, hdr->kernel_size, hdr->second_addr, hdr->second_size)) {
		fprintf(stderr, "error: kernel and second sections will overlap in memory!\n");
		exit(1);
	}
	if (hdr->second_size != 0 && overlap(hdr->ramdisk_addr, hdr->ramdisk_size, hdr->second_addr, hdr->second_size)) {
		fprintf(stderr, "error: ramdisk and second sections will overlap in memory!\n");
		exit(1);
	}

	if (overlap(hdr->kernel_addr, hdr->kernel_size, hdr->tags_addr, hdr->page_size))
		fprintf(stderr, "warning: kernel and tags sections may overlap in memory!\n");
	if (overlap(hdr->ramdisk_addr, hdr->ramdisk_size, hdr->tags_addr, hdr->page_size))
		fprintf(stderr, "warning: ramdisk and tags sections may overlap in memory!\n");
	if (hdr->second_size != 0 && overlap(hdr->second_addr, hdr->second_size, hdr->tags_addr, hdr->page_size))
		fprintf(stderr, "warning: second and tags sections may overlap in memory!\n");
}

int
main(int argc, char **argv)
{
	int i;
	int pgsize;
	FILE *fpout;
	char *outfile;
	char *kernfile, *ramdiskfile, *secondfile;
	uint32_t tagsoff, kernoff, ramdiskoff, secondoff;
	uint32_t sectsize, sectpgs;
	char *sect;
	boot_img_hdr *hdr;

	if (argc != 6 && argc != 7) {
		fprintf(stderr, "usage: %s outfile pagesize tagsoff kernoff:kernfile "
		    "ramdiskoff:ramdiskfile [secondoff:secondfile]\n", argv[0]);
		exit(1);
	}

	outfile = argv[1];
	fpout = fopen(outfile, "w");
	if (fpout == NULL) {
		fprintf(stderr, "error: failed to open output file [%s]\n", outfile);
		exit(1);
	}

	pgsize = strtoul(argv[2], NULL, 0);
	if (ffs(pgsize) == 0 || (pgsize & ~(1 << (ffs(pgsize) - 1)))) {
		fprintf(stderr, "error: non-power-of-2 page size: %d\n", pgsize);
		exit(1);
	}
	if (pgsize < sizeof(boot_img_hdr)) {
		fprintf(stderr, "error: page size too small (must be >= %d)\n",
		    sizeof(boot_img_hdr));
		exit(1); 
	} 

	tagsoff = strtoul(argv[3], NULL, 0);
	kernoff = ramdiskoff = secondoff = 0;
	kernfile = ramdiskfile = secondfile = NULL;
	get_off_and_file(argv[4], &kernoff, &kernfile);
	get_off_and_file(argv[5], &ramdiskoff, &ramdiskfile);
	if (argc == 7)
		get_off_and_file(argv[6], &secondoff, &secondfile);

	hdr = xmalloc(pgsize);
	memset(hdr, 0, sizeof(*hdr));

	/*
	 * Fill out what we can of the header and write it out first. 
	 */
	memcpy(&hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE);
	hdr->kernel_addr = kernoff;
	hdr->ramdisk_addr = ramdiskoff;
	hdr->second_addr = secondoff;
	hdr->tags_addr = tagsoff;
	hdr->page_size = pgsize;
	xfwrite(hdr, pgsize, 1, fpout);

	/*
	 * Load kernel section and write it out.
	 */
	load_section(pgsize, kernfile, &sect, &sectsize, &sectpgs); 
	hdr->kernel_size = sectsize;
	xfwrite(sect, sectpgs * pgsize, 1, fpout);
	free(sect);

	/*
	 * Load ramdisk section and write it out.
	 */
	load_section(pgsize, ramdiskfile, &sect, &sectsize, &sectpgs); 
	hdr->ramdisk_size = sectsize;
	xfwrite(sect, sectpgs * pgsize, 1, fpout);
	free(sect);

	/*
	 * Load secondary section (if provided) and write it out.
	 */
	if (argc == 7) {
		load_section(pgsize, secondfile, &sect, &sectsize, &sectpgs); 
		hdr->second_size = sectsize;
		xfwrite(sect, sectpgs * pgsize, 1, fpout);
		free(sect);
	}

	/*
	 * Write correct header.
	 */
	if (fseek(fpout, 0, SEEK_SET) != 0) {
		perror("fseek failed");
		exit(1);
	}
	xfwrite(hdr, pgsize, 1, fpout);
	
	fclose(fpout);

	/*
	 * Sanity check.
	 */
	sanity_check(hdr);

	return (0);
}
