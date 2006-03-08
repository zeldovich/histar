#ifndef DISKLAYOUT_H_
#define DISKLAYOUT_H_

// all units are in pages
#define HEADER_OFFSET       0
#define HEADER_PAGES        1

#define HEADER_OFFSET2      (HEADER_OFFSET + HEADER_PAGES)
#define HEADER_PAGES2       HEADER_PAGES

#define LOG_OFFSET          (HEADER_OFFSET2 + HEADER_PAGES2)
#define LOG_PAGES           3000

#define RESERVED_PAGES      (HEADER_PAGES + HEADER_PAGES2 + LOG_PAGES)

#endif /*DISKLAYOUT_H_*/
