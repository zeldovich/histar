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

#ifndef REP_BCAST_H
#define REP_BCAST_H 1

#include <arpc.h>

class bcast_info_t {
  ifchgcb_t *ifccb;
  void ifinit ();

public:
  vec<in_addr> bcast_addrs;

  bcast_info_t () : ifccb (NULL) {}
  ~bcast_info_t () { ifchgcb_remove (ifccb); }
  void init () { if (!ifccb) ifinit (); }

  static int bind_bcast_sock (u_int16_t udpport = 0, bool reuseaddr = false);
  static bool get_bcast_addrs (vec<in_addr> *resp);
};

extern bcast_info_t bcast_info;

class rpccb_bcast : public rpccb {
  bool checksrc (const sockaddr *) const { return true; }
  void finish (clnt_stat stat);

public:
  rpccb_bcast (ref<aclnt> c, xdrsuio &x, aclnt_cb cb,
	       void *out, xdrproc_t outproc, sockaddr *dest)
    : rpccb (c, x, cb, out, outproc, dest) {}
  virtual ~rpccb_bcast () {}
  callbase *init (xdrsuio &x);
};

#endif
