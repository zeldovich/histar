// not to be compiled as a seperate translation unit...

#define O_RDWR 		0x00000001
#define O_RDONLY 	0x00000002
#define O_CREAT 	0x00000004
#define O_TRUNC 	0x00000008

#define S_IRWXU 	0x00000001

#define S_IRUSR		0x00000002
#define S_IWUSR		0x00000004

struct stat
{
	int st_uid ;
} ;

int errno = 0 ;

int
umask(int mask)
{
	return 0 ;	
}

int 
time(int *t)
{
	return 0 ;	
}

int
fsync(int fd)
{
	return 0 ;	
}

int
unlink(const char *pathname)
{
	return 0 ;	
}

void
srandom(unsigned int seed)
{
	;
}

long int
random(void)
{
	return 0 ;
}

int
getpid(void)
{
	return 0 ;
}	

int
lseek(int fildes, off_t offset, int whence)
{
	return 0 ;
}

int 
fchown(int fd, int owner, int group)
{
	return 0 ;
}

void
perror(const char *s)
{
	;
}	

void
sync(void)
{
	;
}

int 
fstat(int filedes, struct stat *buf)
{
	return 0 ;
}

int 
creat(const char *pathname, int mode)
{
	return 0 ;
}
