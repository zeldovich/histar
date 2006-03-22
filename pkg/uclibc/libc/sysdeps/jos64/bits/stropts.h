/* Copyright (C) 1998, 1999, 2000, 2002 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef _STROPTS_H
# error "Never include <bits/stropts.h> directly; use <stropts.h> instead."
#endif

#ifndef _BITS_STROPTS_H
#define _BITS_STROPTS_H	1

#include <bits/types.h>

/* Macros used as `request' argument to `ioctl'.  */
#define __SID		('S' << 8)

#define I_NREAD	    (__SID | 1)	/* Counts the number of data bytes in the data
				   block in the first message.  */
#define I_PUSH	    (__SID | 2)	/* Push STREAMS module onto top of the current
				   STREAM, just below the STREAM head.  */
#define I_POP	    (__SID | 3)	/* Remove STREAMS module from just below the
				   STREAM head.  */
#define I_LOOK	    (__SID | 4)	/* Retrieve the name of the module just below
				   the STREAM head and place it in a character
				   string.  */
#define I_FLUSH	    (__SID | 5)	/* Flush all input and/or output.  */
#define I_SRDOPT    (__SID | 6)	/* Sets the read mode.  */
#define I_GRDOPT    (__SID | 7)	/* Returns the current read mode setting.  */
#define I_STR	    (__SID | 8)	/* Construct an internal STREAMS `ioctl'
				   message and send that message downstream. */
#define I_SETSIG    (__SID | 9)	/* Inform the STREAM head that the process
				   wants the SIGPOLL signal issued.  */
#define I_GETSIG    (__SID |10) /* Return the events for which the calling
				   process is currently registered to be sent
				   a SIGPOLL signal.  */
#define I_FIND	    (__SID |11) /* Compares the names of all modules currently
				   present in the STREAM to the name pointed to
				   by `arg'.  */
#define I_LINK	    (__SID |12) /* Connect two STREAMs.  */
#define I_UNLINK    (__SID |13) /* Disconnects the two STREAMs.  */
#define I_PEEK	    (__SID |15) /* Allows a process to retrieve the information
				   in the first message on the STREAM head read
				   queue without taking the message off the
				   queue.  */
#define I_FDINSERT  (__SID |16) /* Create a message from the specified
				   buffer(s), adds information about another
				   STREAM, and send the message downstream.  */
#define I_SENDFD    (__SID |17) /* Requests the STREAM associated with `fildes'
				   to send a message, containing a file
				   pointer, to the STREAM head at the other end
				   of a STREAMS pipe.  */
#define I_RECVFD    (__SID |14) /* Non-EFT definition.  */
#define I_SWROPT    (__SID |19) /* Set the write mode.  */
#define I_GWROPT    (__SID |20) /* Return the current write mode setting.  */
#define I_LIST	    (__SID |21) /* List all the module names on the STREAM, up
				   to and including the topmost driver name. */
#define I_PLINK	    (__SID |22) /* Connect two STREAMs with a persistent
				   link.  */
#define I_PUNLINK   (__SID |23) /* Disconnect the two STREAMs that were
				   connected with a persistent link.  */
#define I_FLUSHBAND (__SID |28) /* Flush only band specified.  */
#define I_CKBAND    (__SID |29) /* Check if the message of a given priority
				   band exists on the STREAM head read
				   queue.  */
#define I_GETBAND   (__SID |30) /* Return the priority band of the first
				   message on the STREAM head read queue.  */
#define I_ATMARK    (__SID |31) /* See if the current message on the STREAM
				   head read queue is "marked" by some module
				   downstream.  */
#define I_SETCLTIME (__SID |32) /* Set the time the STREAM head will delay when
				   a STREAM is closing and there is data on
				   the write queues.  */
#define I_GETCLTIME (__SID |33) /* Get current value for closing timeout.  */
#define I_CANPUT    (__SID |34) /* Check if a certain band is writable.  */

#endif /* bits/stropts.h */
