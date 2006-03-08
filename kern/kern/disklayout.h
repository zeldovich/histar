#ifndef DISKLAYOUT_H_
#define DISKLAYOUT_H_

// all units are in pages
#define HEADER_OFFSET       0
#define HEADER_PAGES      1

#define LOG_OFFSET  (HEADER_OFFSET + HEADER_PAGES)
#define LOG_PAGES    3000
#define LOG_MEMORY  100  // XXX: in disklayout.h?


#endif /*DISKLAYOUT_H_*/
