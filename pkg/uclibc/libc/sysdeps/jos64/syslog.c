#include <bits/unimpl.h>

#include <sys/syslog.h>

/*
 * OPENLOG -- open system log
 */
void
openlog( const char *ident, int logstat, int logfac )
{
    set_enosys();
}

/*
 * syslog, vsyslog --
 *     print message on log file; output is intended for syslogd(8).
 */
void
vsyslog( int pri, const char *fmt, va_list ap )
{
    set_enosys();
}

void
syslog(int pri, const char *fmt, ...)
{
    set_enosys();
}

/*
 * CLOSELOG -- close the system log
 */
void
closelog( void )
{
    set_enosys();
}

/* setlogmask -- set the log mask level */
int setlogmask(int pmask)
{
    int omask = -1;
    set_enosys();
    return omask;
}

