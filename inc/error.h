/* See COPYRIGHT for copyright information. */

#ifndef JOS_INC_ERROR_H
#define JOS_INC_ERROR_H

// Kernel error codes -- keep in sync with list in lib/printfmt.c.
enum {
    E_UNSPEC = 1,	// Unspecified or unknown problem
    E_INVAL,		// Invalid parameter
    E_NO_MEM,		// Request failed due to memory shortage
    E_RESTART,		// Restart system call
    E_NOT_FOUND,	// Object not found
    E_LABEL,		// label check error
    E_BUSY,		// device busy
    E_NO_SPACE,		// not enough space in buffer
    E_AGAIN,		// try again
    E_IO,		// disk IO error
    E_FIXEDSIZE,	// object is fixed-size
    E_VARSIZE,		// object is variable-sized

    // user-space errors
    E_RANGE,		// value out of range
    E_EOF,		// unexpected end-of-file
    E_MAX_OPEN,		// out of file descriptors
    E_BAD_OP,		// operation not supported
    E_EXISTS,		// already exists
    E_MAXERROR
};

#endif // _ERROR_H_
