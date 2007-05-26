extern "C" {
#include <inc/lib.h>
#include <inc/netd.h>
#include <inc/string.h>
#include <inc/fs.h>
#include <inc/syscall.h>
#include <inc/assert.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
}

#include <inc/nethelper.hh>
#include <inc/error.hh>

static int fetch_debug = 1;

int
main(int ac, char **av)
try
{
    if (ac != 3) {
	printf("Usage: %s url filename\n", av[0]);
	return -1;
    }

    const char *ustr = av[1];
    char *pn = av[2];

    printf("Fetching %s into %s\n", ustr, pn);

    url u(av[1]);
    if (fetch_debug)
	printf("URL: host %s path %s\n", u.host(), u.path());

    tcpconn tc(u.host(), 80);
    if (fetch_debug)
	printf("Connected OK\n");

    char buf[1024];
    sprintf(buf, "GET %s HTTP/1.0\r\n\r\n", u.path());
    tc.write(buf, strlen(buf));
    if (fetch_debug)
	printf("Sent request OK\n");

    lineparser lp(&tc);
    const char *resp = lp.read_line();
    assert(resp);
    if (strncmp(resp, "HTTP/1.", 7))
	throw basic_exception("not an HTTP response: %s", resp);
    if (strncmp(&resp[9], "200", 3))
	throw basic_exception("request error: %s", resp);

    while (resp[0] != '\0') {
	resp = lp.read_line();
	assert(resp);
    }

    const char *dirname, *basename;
    fs_dirbase(pn, &dirname, &basename);

    struct fs_inode dir;
    int r = fs_namei(dirname, &dir);
    if (r < 0)
	throw error(r, "cannot find directory %s: %s", dirname, e2s(r));

    struct fs_inode file;
    r = fs_create(dir, basename, &file, 0);
    if (r < 0)
	throw error(r, "cannot create file %s", basename);

    uint64_t off = 0;
    for (;;) {
	size_t cc = lp.read(buf, sizeof(buf));
	if (cc == 0)
	    break;

	ssize_t cr = fs_pwrite(file, buf, cc, off);
	if (cr < 0)
	    throw error(cr, "fs_pwrite");

	off += cc;

	static size_t reported;
	if (off > reported + 256 * 1024) {
	    printf("Fetched %"PRIu64" bytes\n", off);
	    reported = off;
	}
    }

    printf("Done.\n");
} catch (std::exception &e) {
    printf("Error: %s\n", e.what());
}
