/*
 *
 * Copyright (C) 2006 David Mazieres (dm@uun.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 */

#include <net/if.h>
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif /* HAVE_SYS_SOCKIO_H */
#include "bcast.hh"

bcast_info_t bcast_info;

void
bcast_info_t::ifinit ()
{
  if (!ifccb)
    ifccb = ifchgcb (wrap (this, &bcast_info_t::ifinit));
  get_bcast_addrs (&bcast_addrs);
}

int
bcast_info_t::bind_bcast_sock (u_int16_t udpport, bool reuseaddr)
{
  int fd;
  struct sockaddr_in sin;
  int one = 1;

  bzero (&sin, sizeof (sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons (udpport);
  sin.sin_addr.s_addr = htonl (INADDR_ANY);
  fd = socket (AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    warn ("UDP socket: %m\n");
    return -1;
  }

  if (reuseaddr) {
    int n = 1;
    setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, (char *) &n, sizeof (n));
#ifdef SO_REUSEPORT
    n = 1;
    setsockopt (fd, SOL_SOCKET, SO_REUSEPORT, (char *) &n, sizeof (n));
#endif /* SO_REUSEPORT */
  }

  if (bind (fd, (struct sockaddr *) &sin, sizeof (sin)) < 0) {
    warn ("bind UDP port %d: %m\n", udpport);
    close (fd);
    return -1;
  }
  if (setsockopt (fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof (one)) < 0) {
    perror ("SO_BROADCAST: %m");
    close (fd);
    return -1;
  }

  return fd;
}

bool
bcast_info_t::get_bcast_addrs (vec<in_addr> *resp)
{
  int s;
  struct ifconf ifc;
  char buf[8192];
  int naddr = 0;
  struct in_addr *addrs = NULL;
  char *p, *e;

  s = socket (AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    warn ("socket: %m");
    return false;
  }

  ifc.ifc_len = sizeof (buf);
  ifc.ifc_buf = buf;
  if (ioctl (s, SIOCGIFCONF, &ifc) < 0) {
    warn ("SIOCGIFCONF: %m");
    close (s);
    return false;
  }
  if (ifc.ifc_len + sizeof (struct ifreq) + 64 >= sizeof (buf)) {
    warn ("SIOGIFCONF: response too long\n");
    close (s);
    return false;
  }

  p = ifc.ifc_buf;
  e = p + ifc.ifc_len;
  while (p < e) {
    struct ifreq ifr, *ifrp;
    struct in_addr a;
    int i;

    ifrp = (struct ifreq *) p;
    ifr = *ifrp;
#if HAVE_SA_LEN
    if (ifrp->ifr_addr.sa_len > sizeof (ifrp->ifr_addr))
      p += sizeof (ifrp->ifr_name) + ifrp->ifr_addr.sa_len;
    else
#endif /* HAVE_SA_LEN */
      p += sizeof (ifr);
    if (ifrp->ifr_addr.sa_family != AF_INET)
      continue;
    if (ioctl (s, SIOCGIFFLAGS, &ifr) < 0) {
      warn ("SIOCGIFFLAGS: %m");
      close (s);
      free (addrs);
      return false;
    }
    if ((ifr.ifr_flags & (IFF_UP|IFF_RUNNING|IFF_BROADCAST))
	!= (IFF_UP|IFF_RUNNING|IFF_BROADCAST))
      continue;
    if (ioctl (s, SIOCGIFBRDADDR, &ifr) < 0) {
      warn ("SIOCGIFBRADDR: %m");
      close (s);
      free (addrs);
      return false;
    }
    if (ifr.ifr_broadaddr.sa_family != AF_INET)
      continue;
    a = ((struct sockaddr_in *) &ifr.ifr_broadaddr)->sin_addr;
    for (i = 0; i < naddr && addrs[i].s_addr != a.s_addr; i++)
      ;
    if (i >= naddr && a.s_addr != htonl (INADDR_ANY)) {
      addrs = (struct in_addr *) realloc (addrs, ++naddr * sizeof (addrs[0]));
      if (!addrs)
	panic ("out of memory in %s\n", __FL__);
      addrs[naddr - 1] = a;
    }
  }

  close (s);
  resp->setsize (naddr);
  for (int i = 0; i < naddr; i++)
    (*resp)[i] = addrs[i];
  free (addrs);
  return true;
}


void
rpccb_bcast::finish (clnt_stat stat)
{
  aclnt_cb c (cb);
  switch (stat) {
  case RPC_CANTDECODERES:
  case RPC_VERSMISMATCH:
  case RPC_PROGUNAVAIL:
  case RPC_PROGVERSMISMATCH:
  case RPC_CANTDECODEARGS:
    return;
  case RPC_TIMEDOUT:
    delete this;
    break;
  default:
    break;
  }
  (*c) (stat);
}

callbase *
rpccb_bcast::init (xdrsuio &x)
{
  ref<aclnt> cc (c);

  if (dest->sa_family != AF_INET)
    panic ("don't know how to broadcast to non-IPV4 socket\n");

  struct sockaddr_in sin = *reinterpret_cast<const sockaddr_in *> (dest);
  if (sin.sin_port == htons (0))
    panic ("don't know how to broadcast to port 0\n");

  if (sin.sin_addr.s_addr == htonl (INADDR_BROADCAST)) {
    bcast_info.init ();
    for (const in_addr *ap = bcast_info.bcast_addrs.base ();
	 ap < bcast_info.bcast_addrs.lim (); ap++) {
      sin.sin_addr = *ap;
      cc->xprt ()->sendv (x.iov (), x.iovcnt (),
			  reinterpret_cast<sockaddr *> (&sin));
      if (cc->xi_xh_ateof_fail ())
	return NULL;
    }
  }
  else {
    cc->xprt ()->sendv (x.iov (), x.iovcnt (),
			reinterpret_cast<sockaddr *> (&sin));
    if (cc->xi_xh_ateof_fail ())
      return NULL;
  }

  return this;
}
