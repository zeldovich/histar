/* See COPYRIGHT for copyright information. */

#ifndef JOS_INC_ERROR_H
#define JOS_INC_ERROR_H

// Kernel error codes -- keep in sync with list in lib/printfmt.c.
#define E_UNSPECIFIED	1	// Unspecified or unknown problem
#define E_BAD_ENV       2       // Environment doesn't exist or otherwise
				// cannot be used in requested action
#define E_INVAL		3	// Invalid parameter
#define E_FAULT		4	// Bad memory passed to kernel
#define E_NO_MEM	5	// Request failed due to memory shortage
#define E_NO_FREE_ENV	6	// Attempt to create a new environment beyond
				// the maximum allowed
#define E_IPC_NOT_RECV	7	// Attempt to send to env that is not recving
#define E_EOF		8	// Unexpected end of file

// File system error codes -- only seen in user-level
#define	E_NO_DISK	9	// No free space left on disk
#define E_MAX_OPEN	10	// Too many files are open
#define E_NOT_FOUND	11 	// File or block not found
#define E_BAD_PATH	12	// Bad path
#define E_FILE_EXISTS	13	// File already exists
#define E_NOT_EXEC	14	// File not a valid executable

#define E_AGAIN		15	// Temporary failure

#define E_RANGE		16	// out of range

#define E_LABEL		17	// label check error

#define E_BUSY		18	// device busy
#define E_NO_DEV	19	// device does not exist

#define E_NET_ABRT	20	// Net connection aborted
#define E_NET_RST	21	// Net connection reset
#define E_NET_CONN	22	// No connection
#define E_NET_USE	23	// Net address in use
#define E_NET_IF	24	// Net low-level netif error

#define E_IN_PROGRESS	25	// operation in progress

#define E_NO_SPACE	26	// not enough space in upcall area
//  Hmmm, is maybe an error?  for now it's not.
#define E_MAYBE		27	// return "maybe"
#define E_DEST_FAULT	28	// destination upcall area fault
#define E_PERM		29	// permission
#define MAXERROR	30

#endif // _ERROR_H_
