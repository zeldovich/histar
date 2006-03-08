#ifndef JOS_INC_FCNTL_H
#define JOS_INC_FCNTL_H

int  fcntl(int fd, int cmd, ...);

#define F_DUPFD         0       /* Duplicate file descriptor.  */
#define F_GETFD         1       /* Get file descriptor flags.  */
#define F_SETFD         2       /* Set file descriptor flags.  */
#define F_GETFL		3
#define F_SETFL		4

#define FD_CLOEXEC      1       /* actually anything with low bit set goes */

#define O_RDONLY        0x0000
#define O_WRONLY        0x0001
#define O_RDWR          0x0002
#define O_ACCMODE       (O_RDONLY | O_RDWR | O_WRONLY)
#define O_CREAT         0x0040

// not implemented yet
#define O_EXCL		0x0080
#define O_TRUNC		0x0200
#define O_APPEND	0x0400

#endif
