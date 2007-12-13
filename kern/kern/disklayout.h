#ifndef DISKLAYOUT_H_
#define DISKLAYOUT_H_

// all units are in pages
#define HEADER_OFFSET       0
#define HEADER_PAGES        1

#define LOG_OFFSET          (HEADER_OFFSET + HEADER_PAGES)
#define MAX_LOG_PAGES       131072U

/*
 * Conservatively, log_pages = disk_bytes / 65536, but in reality we can
 * only write to the log what we have in memory, so 131072 log pages is
 * enough for ~8GB dirty memory.
 */

#endif /*DISKLAYOUT_H_*/
