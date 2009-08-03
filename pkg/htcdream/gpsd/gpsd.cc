extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <inc/stdio.h>

#include "gps.h"

#include "../rild/support/misc.h"
}

#define LIBGPS_SO	"/bin/libgps.so"

static void
location_callback(GpsLocation *location)
{
	fprintf(stderr, "::%s::\n", __func__);
	fprintf(stderr, "  latitude:  %f\n", location->latitude);
	fprintf(stderr, "  longitude: %f\n", location->latitude);
}

static void
status_callback(GpsStatus *status)
{
	fprintf(stderr, "::%s::\n", __func__);
	fprintf(stderr, "  status: %d\n", status->status);
}

static void
sv_status_callback(GpsSvStatus *sv_info)
{
	fprintf(stderr, "::%s::\n", __func__);
	fprintf(stderr, "  number of SVs:  %d\n", sv_info->num_svs);
}

static void
download_request_callback()
{
	fprintf(stderr, "--Xtra download request CB\n");
}

GpsXtraCallbacks sGpsXtraCallbacks = {
	download_request_callback,
};

int
main(int argc, char **argv)
{
	const GpsInterface *(*gps_get_hardware_interface)(void);
	GpsCallbacks cbs = {
		location_callback, status_callback, sv_status_callback
	};
	void *dlHandle;

	dlHandle = dlopen(LIBGPS_SO, RTLD_NOW);
	if (dlHandle == NULL) {
		fprintf(stderr, "gpsd: dlopen failed: %s\n", dlerror());
		exit(-1);
	}
	fprintf(stderr, "%s loaded.\n", LIBGPS_SO);

	gps_get_hardware_interface = (const GpsInterface *(*)(void))
	    dlsym(dlHandle, "gps_get_hardware_interface");
	if (gps_get_hardware_interface == NULL) {
		fprintf(stderr, "gpsd: gps_get_hardware_interface not "
		    "defined or exported in %s\n", LIBGPS_SO);
		exit(1);
	}

	fprintf(stderr, "gps_get_hardware_interface at %p\n",
	    gps_get_hardware_interface);
	fprintf(stderr, "Calling gps_get_hardware_interface()\n");
	const GpsInterface *iface = gps_get_hardware_interface();

	fprintf(stderr, "GpsInterface at %p, iface->init at %p, "
	    "iface->start at %p, iface->stop at %p\n", iface,
	    iface->init, iface->start, iface->stop);

	iface->init(&cbs);
	fprintf(stderr, "GpsInit done.\n");
	const GpsXtraInterface *xtra = (const GpsXtraInterface*)
	    iface->get_extension(GPS_XTRA_INTERFACE);
	if (xtra != NULL) {
		fprintf(stderr, "Gps supports Xtra interface\n");
		int r = xtra->init(&sGpsXtraCallbacks);
		if (r)
			fprintf(stderr, " -- Xtra interface init failed: %d\n", r);
	} else {
		fprintf(stderr, "Xtra interface not supported\n");
	}

	iface->delete_aiding_data(GPS_DELETE_ALL);
	iface->set_position_mode(GPS_POSITION_MODE_STANDALONE, 5);
	iface->start();

	fprintf(stderr, "opening NMEA tty\n");
	smd_tty_open(27); 

	while (1) {
		unsigned char buf[512];
		smd_tty_read(27, buf, sizeof(buf));
		fprintf(stderr, "NMEA: [%s]\n", buf);
	}
}
