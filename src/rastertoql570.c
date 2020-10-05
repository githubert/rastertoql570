/* rastertoql570.c: a CUPS driver for the QL-570 label printer
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

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <cups/cups.h>
#include <cups/raster.h>
#include <cups/sidechannel.h>

#include "ql570.h"
#include "rastertoql570.h"

int
main(__attribute__((unused)) int argc, __attribute__((unused)) char** argv)
{
	// As per recommendation in the CUPS documentation 
	// TODO: use sigaction()
	signal(SIGPIPE, SIG_IGN);

	FILE *fout = fdopen(STDOUT_FILENO, "wb");

	if (fout == NULL) {
		fprintf(stderr, "CRIT: Error while opening file.\n");
		return 1;
	}

	// NOTE: Currently the resulting update of `status` by `init` (below)
	// is not used. We still query for some status in order to determine if
	// the printer is responding. Also, at a later stage, we can use the
	// information in `status` to determine the printer type and set some
	// variables, such as the raster buffer size, the minimal raster line
	// count, etc.
	ql_status status = { 0 };

	if (!init(&status, fout)) {
		fprintf(stderr, "CRIT: Could not get status information.\n");
		return 1;
	}

	cups_raster_t *raster = cupsRasterOpen(0, CUPS_RASTER_READ);
	cups_page_header2_t header;
	unsigned int page_counter = 0;

	while (cupsRasterReadHeader2(raster, &header)) {
		handle_page(raster, header, status, page_counter, fout);
		page_counter++;

		// Printing this information will also end up on the jobs page
		// of the CUPS web interface. I've seen a lot of printers that
		// do not include this information and so the "Pages" number
		// will end up being "Unknown".
		fprintf(stderr, "PAGE: %d #-pages\n", page_counter);
	}

	cupsRasterClose(raster);
	fclose(fout);

	return EXIT_SUCCESS;
}

/**
 * Print a page.
 *
 * This was factored out from main(), so (for the moment) it is a bit unwieldy
 * with regards to parameters.
 *
 */
void
handle_page(cups_raster_t *raster, cups_page_header2_t header, ql_status status, unsigned int page_counter, FILE *fout)
{
	/* // TODO: Support some safety option for testing.
	if( header.cupsHeight > 900 )
		header.cupsHeight = 900;
	*/

	uint32_t cupsHeight = header.cupsHeight;

	// Enforce a minimum of 150 lines.
	if( cupsHeight < 150 ) {
		cupsHeight = 150;
	}

	ql_print_info print_info = {
		.valid_flag = PIV_QUALITY,
		.raster_number[0] = cupsHeight & 0x00FF,
		.raster_number[1] = (cupsHeight & 0xFF00) >> 8,
		.successive_page = page_counter > 0
	};

	ql_page_start(&print_info, fout);

	if (header.HWResolution[1] == 600)
		ql_set_extended_options(true, true, fout);
	else
		ql_set_extended_options(true, false, fout);

	uint8_t buffer[header.cupsBytesPerLine];
	size_t output_buffer_size = 90; // TODO: Depends on printer type
	uint8_t output_buffer[output_buffer_size];
	memset(output_buffer, 0x00, output_buffer_size);

	/*
	 * We insert blank lines before and after the raster output, if the
	 * line count of the original raster data is below 150.
	 *
	 * TODO: Depends on printer type, some other types need 295 lines.
	 */
	int blanks = 150 - header.cupsHeight;

	if (blanks > 0)
		print_blank_lines(blanks / 2, output_buffer_size, fout);

	for (unsigned int i = 0; i < header.cupsHeight; ++i) {
		if(cupsRasterReadPixels(raster, buffer, header.cupsBytesPerLine) == 0)
			break;

		// In the (undesirable) case that the input_buffer is smaller than
		// the output_buffer
		if (header.cupsBytesPerLine < output_buffer_size)
			memcpy(output_buffer, buffer, header.cupsBytesPerLine);
		else
			memcpy(output_buffer, buffer, output_buffer_size);

		ql_raster(output_buffer_size, output_buffer, fout);
	}

	if (blanks > 0)
		print_blank_lines(blanks / 2 + (blanks % 2), output_buffer_size, fout);

	ql_raster_end(output_buffer_size, fout);

	// TODO: Determine total number of pages and correctly indicate last page.
	ql_page_end(false, fout);
	
	// Give the printer a moment to return status data.
	nanosleep(&(struct timespec){0, 100e6}, NULL);

	wait_for_page_end();
}

/**
 * Wait for a status indicating that the next page can be sent.
 *
 * This function tries to follow the printer status after a page has been
 * submitted.  It will try up to 25 times to read a status struct and find out
 * if an end state has been reached.
 */
void
wait_for_page_end()
{
	ql_status status = {0};

	for (uint8_t i = 0; i < 25; i++) {
		// Sleep and skip in case of error.
		if (!backchannel_read_status(&status)) {
			nanosleep(&(struct timespec){0, 100e6}, NULL);
			fprintf(stderr, "ERROR: Backchannel short read, retrying.\n");
			continue;
		}
		
		// Skip this round if data seems to be corrupt.
		if (status.print_head_mark != 0x80) {
			fprintf(stderr, "ERROR: Print status returned is invalid, retrying.\n");
			continue;
		}

		if (handle_status(&status))
			return;
	}
}


/**
 * Turns status information into user visible information.
 *
 * This function returns true to indicate that an end state has been reached.
 * This means either an error occurred or the printer changed to PT_WAITING
 * (ready state).
 *
 * TODO: Maybe split this into further pieces, just look at that switch beneath
 * a case!
 *
 * @param status the printer status to be inspected
 * @returns true to indicate that polling can be stopped.
 */
bool
handle_status(ql_status *status)
{
	switch(status->status_type) {
	case ST_COMPLETED:
		fprintf(stderr, "INFO: Page completed.\n");
		break;

	// TODO: Reduce general ugliness (the problem is that multiple error
	// conditions might exist at the same time)
	case ST_ERROR:
		if(status->error_info_1 & NO_MEDIA)
			fprintf(stderr, "ERROR: No media.\n");

		if(status->error_info_1 & END_OF_MEDIA)
			fprintf(stderr, "ERROR: End of media.\n");

		if(status->error_info_1 & TAPE_CUTTER_JAM)
			fprintf(stderr, "ERROR: Tape cutter jam.\n");

		if(status->error_info_1 & MAIN_UNIT_IN_USE)
			fprintf(stderr, "ERROR: Main unit in use.\n");

		if(status->error_info_1 & FAN_MALFUNCTION)
			fprintf(stderr, "ERROR: Fan malfunction.\n");

		if(status->error_info_2 & WRONG_MEDIA)
			fprintf(stderr, "ERROR: Wrong media.\n");

		if(status->error_info_2 & TRANSMISSION_ERROR)
			fprintf(stderr, "ERROR: Transmission error.\n");

		if(status->error_info_2 & COVER_OPENED)
			fprintf(stderr, "ERROR: Cover opened.\n");

		if(status->error_info_2 & CANNOT_FEED)
			fprintf(stderr, "ERROR: Cannot feed.\n");

		return true; // stop polling

	case ST_NOTIFICATION:
		switch (status->notification_type) {
		case NT_COOLING_STARTED:
			fprintf(stderr, "INFO: Cooling started.\n");
			break;

		case NT_COOLING_FINISHED:
			fprintf(stderr, "INFO: Cooling finished.\n");
			break;
		}
		break;

	case ST_PHASE_CHANGE:
		switch(status->phase_type) {
		case PT_WAITING:
			fprintf(stderr, "INFO: Ready.\n");
			return true; // stop polling

		case PT_PRINTING:
			fprintf(stderr, "INFO: Printing...\n");
			break;
		}
		break;
	}

	return false;
}

/**
 * Initialise the printer.
 *
 * This will try a few times to initialise the printer. After the first try,
 * ql_init() will be called with flush=true. We will sleep for 100ms after each
 * call to ql_init(), because I observed that sometimes data is not ready, but
 * the timeout on cupsBackChannelRead will no come into effect. I guess I am
 * misunderstanding something with regards to the timeout.
 *
 * TODO: Understand timeout, rewrite above paragraph. It is mucho ugly.
 *
 * @param status status struct to populate with a response from the printer
 * @param device file descriptor to write to
 */
bool
init(ql_status *status, FILE* device)
{
	bool flush = false;

	for(int i = 0; i < 10; ++i) {
		ql_init(flush, device);

		nanosleep(&(struct timespec){0, 100e6}, NULL);

		if (!request_status(status, device))
			continue;

		if (status->print_head_mark == 0x80)
			return true;

		// Flush printer buffers on subsequent retries.
		flush = true;
	}

	return false;
}

/**
 * Read status information from backchannel.
 *
 * @param status status struct to fill
 * @returns false if number of bytes read was not `sizeof(ql_status)`
 */
bool
backchannel_read_status(ql_status* status)
{
	ssize_t ret = cupsBackChannelRead((uint8_t*)status, sizeof(ql_status), 10.0);

	if (ret != sizeof(ql_status))
		return false;

	return true;
}

/**
 * Insert blank lines into raster data.
 *
 * @param count number of lines
 * @param buffer_size size of the raster line (e.g. 90)
 * @param device file descriptor to write to
 */
void
print_blank_lines(uint32_t count, size_t buffer_size, FILE *device)
{
	uint8_t buffer[buffer_size];
	memset(buffer, 0x00, buffer_size);

	for(uint32_t i = 0; i < count; i++)
		ql_raster(buffer_size, buffer, device);
}

/**
 * Request status and read response from backchannel.
 *
 * @param status status struct to fill
 * @returns status of backchannel_read_status() operation
 */
bool
request_status(ql_status *status, FILE *device)
{
	// Send status request
	ql_status_request(device);

	// Read response
	return backchannel_read_status(status);
}
