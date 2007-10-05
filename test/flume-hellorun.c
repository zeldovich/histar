#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>

#include "flume_cpp.h"
#include "flume_prot.h"
#include "flume_api.h"
#include "flume_clnt.h"

static char *prog_name;
extern char **environ;

int
main (int argc, char *argv[])
{
  prog_name = argv[0];

  x_handle_t pid;
  int forw, rev, rc;
  const char *nargv[6];
  x_handlevec_t *fdhandles = label_alloc (2);
  x_labelset_t *ls = labelset_alloc ();

  if ((rc = flume_socketpair (DUPLEX_ME_TO_THEM, &forw, &fdhandles->val[0], "forward")) < 0) {
    fprintf (stderr, "flume_socketpair1 error errno %d\n", errno);
    exit (1);
  }

  if ((rc = flume_socketpair (DUPLEX_THEM_TO_ME, &rev, &fdhandles->val[1], "reverse")) < 0) {
    fprintf (stderr, "flume_socketpair2 error errno %d\n", errno);
    exit (1);
  }

  x_handle_t shandle;
  if ((rc = flume_new_handle (&shandle, 0 /*HANDLE_OPT_DEFAULT_ADD*/, "shandle")) < 0) {
    fprintf (stderr, "flume_new_handle err\n");
    exit (1);
  }

  x_label_t *slabel = label_alloc(1);
  label_set(slabel, 0, shandle);
  labelset_set_S (ls, slabel);

  if ((rc = flume_set_fd_label(slabel, LABEL_S, forw)) < 0) {
    fprintf (stderr, "flume_set_fd_label err1\n");
    exit (1);
  }

  if ((rc = flume_set_fd_label(slabel, LABEL_S, rev)) < 0) {
    fprintf (stderr, "flume_set_fd_label err2\n");
    exit (1);
  }

  nargv[0] = "/disk/nickolai/flume/run/bin/flumeperl";
  nargv[1] = "-e";
  nargv[2] = "print 3;\n";
  nargv[3] = NULL;

  /* XXX for some reason, I cannot pass arguments here? */
  nargv[1] = 0;

  rc = flume_spawn (&pid, nargv[0], (char *const*) nargv,
                   environ, 2, 0, 
                   ls, fdhandles, NULL);
  if (rc < 0) {
    fprintf (stderr, "flume_spawn error errno %d\n", errno);
    exit (1);
  }

  char *msg = "print 5;\n";
  write (forw, msg, strlen(msg));
  close (forw);

  char buf[4096];
  for (;;) {
    ssize_t cc = read(rev, &buf[0], sizeof(buf));
    printf("read rv: %d\n", cc);
    if (cc < 0) {
      fprintf (stderr, "read: %s\n", strerror(errno));
      break;
    }
    if (cc == 0)
      break;

    write(1, &buf[0], cc);
  }

  wait (&rc);
  return 0;
}
