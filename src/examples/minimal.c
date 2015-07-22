/* minimal.c: a minmal example for using the QL-570 utility functions
 *
 * Copyright (C) 2015 Clemens Fries <github-raster@xenoworld.de>
 *
 * This file is part of rastertoql570.
 *
 * rastertoql570 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * rastertoql570 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with rastertoql570.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../ql570.h"
#include <stdlib.h>
#include <errno.h>

/**
 * Raster line length for the QL-570 is 90 bytes. There are some other QL
 * series printers that need a larger buffer.
 */
#define BUFFER_SIZE 90

/**
 * Minimal example program for direct printer access.
 *
 * For a better understanding of what goes on here, please refer
 * to the documentation of each function. At times there are
 * interesting bits of information over there.
 *
 */
int main()
{
	char *devname = "/dev/usb/lp0";
	FILE *device = fopen(devname, "wb");

	if (device == NULL) {
		fprintf(stderr, "Error while opening %s: %s\n", devname, strerror(errno));
		return EXIT_FAILURE;
	}

	ql_status status = {0};
	uint8_t buffer[BUFFER_SIZE];

	/*
	 * Request a print job with 150 lines (the minimal amount of lines for
	 * the QL-570). Note that this number is mandatory. If you print less
	 * than 150 lines, or indicate some wrong amount of lines here, the
	 * printer will just resort to blinking its red LED while observing you
	 * with displeasure.
	 */
	ql_print_info print_info = { .raster_number[0] = 150 };

	/*
	 * Initialise printer. The first flag indicates that we won't be
	 * flushing the printer with 200 bytes of zeroes.
	 */
	ql_init(false, device);

	/*
	 * Ask the printer to give us a status report.
	 */
	ql_status_request(device);

	/*
	 * Retrieve the status report.
	 */
	ql_status_read(&status, device);

	/*
	 * Send the contents of `print_info` to the printer.
	 */
	ql_page_start(&print_info, device);

	/*
	 * Set some extended options: cut-at-end plus 300dpi printing.
	 */
	ql_set_extended_options(true, false, device);
	
	/*
	 * Print some black lines.
	 */
	for (int i = 0; i < 150; i++) {

		/*
		 * Every now and then: Many, many 1s.
		 */
	        memset(buffer, i % 5 == 0 ? 0xFF : 0x00, BUFFER_SIZE);

		/*
		 * We empty the first two bytes in order to force a small
		 * margin. Note that for 62mm the right and left margin is
		 * about 12 dots. I observed that I have a margin any way on
		 * one side, but it looks like it is printing outside the label
		 * area on the other side. It's not that important, but I do
		 * it anyway because I'm not sure about the mechanics and how
		 * well the print heads cope with doing their thing outside
		 * the printable area.
		 *
		 */
		buffer[0] = 0x00;
		buffer[1] = 0x00;

		/*
		 * Send this raster line to the printer.
		 */
	        ql_raster(BUFFER_SIZE, buffer, device);
	}
	
	/*
	 * Indicate the end of raster data to the printer.
	 */
	ql_raster_end(BUFFER_SIZE, device);

	/*
	 * Tell it that this was the last page.
	 */
	ql_page_end(true, device);

	fclose(device);
}
