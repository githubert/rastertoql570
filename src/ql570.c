/* ql570.c: facilities for communicating with the QL-570 label printer
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

#include "ql570.h"

/**
 * Request status from printer.
 *
 * @param device file descriptor to write to
 */
void
ql_status_request(FILE *device)
{
	uint8_t request[3] = {QL_ESC, 0x69, 0x53};
	fwrite(request, 3, 1, device);
	fflush(device);
}

/**
 * Read a status response from the printer.
 *
 * @param status ql_status struct holding the response
 * @param device file descriptor to write to
 * @return true if successful, false if short read
 */
bool
ql_status_read(ql_status *status, FILE *device)
{
	size_t len = fread(status, sizeof(ql_status), 1, device);

	if (len != sizeof(ql_status)) {
		return false;
	}

	return true;
}

/**
 * Initialise printer.
 *
 * The protocol specification has an optional recommendation to flush lingering
 * partial commands with 200 bytes of 0x00.
 *
 * @param flush prepend 200 bytes of invalid (0x00) data
 * @param device file descriptor to write to
 */
void
ql_init(bool flush, FILE *device)
{
	if (flush) {
		uint8_t init_buffer[200];
		memset(&init_buffer, 0x0, 200);
		fwrite(init_buffer, sizeof(uint8_t), 200, device);
		fflush(device);
	}

	uint8_t request[2] = {QL_ESC, 0x40};
	fwrite(request, 2, 1, device);
	fflush(device);
}

/**
 * Write one line of raster data to the printer.
 *
 * For most printers in the QL series one raster line is at most 90 bytes long.
 * As these are monochrome printers this means a line has 720 pixels (at most).
 *
 * @param length length of the raster data
 * @param data pointer to the raster data
 * @param device file descriptor to write to
 */
void
ql_raster(uint8_t length, uint8_t *data, FILE *device)
{
	uint8_t request[3] = {0x67, 0x00, length};
	fwrite(request, 3, 1, device);
	fwrite(data, length, 1, device);
	fflush(device);
}

/**
 * Signal end of raster data.
 *
 * @param length length of raster data (0x00)
 * @param device file descriptor to write to
 */
void
ql_raster_end(uint8_t length, FILE *device)
{
	uint8_t request[3] = {0x67, 0xFF, length};

	uint8_t data[length];
	memset(&data, 0x00, length);

	fwrite(request, 3, 1, device);
	fwrite(data, length, 1, device);

	fflush(device);
}

/**
 * Start a new page.
 *
 * This will write out the contents of the print_info pointer to the printer.
 *
 * @param print_info print_info
 * @param device file descriptor to write to
 */
void
ql_page_start(ql_print_info *print_info, FILE *device)
{
	uint8_t request[3] = {QL_ESC, 0x69, 0x7A};
	fwrite(request, 3, 1, device);
	fwrite(print_info, sizeof(ql_print_info), 1, device);
	fflush(device);
}

/**
 * End the current page.
 *
 * @param last_page indicate that this is the last page
 * @param device file descriptor to write to
 */
void
ql_page_end(bool last_page, FILE *device)
{
	uint8_t request;

	if (last_page) {
		request = 0x1A;
	} else {
		request = 0x0C;
	}

	fwrite(&request, 1, 1, device);
	fflush(device);
}


/**
 * Set extended options.
 *
 * This is called 'set expanded mode' in the specification.
 *
 * ### Cutting
 *
 * Pain points with the specification:
 *
 * * The diagram is wrong. The description says, this is bit 3, the diagram
 *   says, this is bit 4. This cost me some time.
 *   TODO: Check 720N/NW. The documentation there lacks the diagram and the
 *   description sys it is bit 4. I bet this is wrong!
 * * Not cutting is the default. The description says that 'cut at end' is the
 *   default.
 *
 * TODO: This seems to cut after each page, regardless of whether last_page is
 * set. I don't get it.
 *
 * ### High resolution printing
 *
 * Note that this is the resolution along the label _length_. The resolution
 * along the _width_ of the media will stay the same. This means that more
 * lines will be squeezed in the same space.
 *
 * Selecting high resolution printing, will allow the shortest label produced
 * to be shrunk from 12.7mm to 6.35mm. The minimum label length is 150 lines,
 * doubling the resolution will halve the length.
 *
 * @param cut_at_end
 * @param high_resolution false: 300x300 dpi, true: 300x600 dpi
 * @param device file descriptor to write to
 */
void
ql_set_extended_options(bool cut_at_end, bool high_resolution, FILE *device)
{
	uint8_t options = 0x00;

	if (cut_at_end)
		options |= OPT_CUT_AT_END;

	if (high_resolution)
		options |= OPT_HIGH_RESOLUTION;

	uint8_t request[4] = {QL_ESC, 0x69, 0x4B, options};
	fwrite(request, 4, 1, device);
	fflush(device);
}

/**
 * Enable automatic label cutting.
 *
 * The specification calls this "set each mode". The bits of the last byte in
 * the command sequence are either described as unused or undefined, setting
 * bit 6 enables auto cut.
 *
 * @param device file descriptor to write to
 */
void
ql_autocut_enable(FILE *device)
{
	uint8_t request[4] = {QL_ESC, 0x69, 0x4D, 0b01000000};
	fwrite(request, 4, 1, device);
	fflush(device);
}

/**
 * Cut after each _n_ labels.
 *
 * For continuous length labels it makes sense to cut after every page. For 
 * other label types it might make sense to cut after several labels.
 *
 * TODO: default is 1, does that work? If yes, note here
 *
 * @param interval cut after _interval_ number of pages
 * @param device file descriptor to write to
 */
void
ql_autocut_interval(uint8_t interval, FILE *device)
{
	uint8_t request[4] = {QL_ESC, 0x69, 0x41, interval};
	fwrite(request, 4, 1, device);
	fflush(device);
}

/**
 * Sets the default margins.
 *
 * The specification recommends setting the margins to 35 for several printer
 * types, including the QL-570. For other types this is not indicated. Setting
 * this seems to be optional.
 *
 * If the media is 'die cut', then the margins will always be 0.
 *
 * See ql_set_margins for details.
 *
 * @param
 * @param device file descriptor to write to
 *
 */
void
ql_set_default_margins(enum ql_media_type media_type, FILE *device)
{
	if (media_type == MT_CONTINUOUS)
		ql_set_margins(35, device);
	else
		ql_set_margins(0, device);
}

/**
 * Set margins on continuous tape.
 *
 * This adds _margins_ amount of blank lines before and after the label.
 *
 * @param margins number of blank lines
 * @param device file descriptor to write to
 */
void
ql_set_margins(uint16_t margins, FILE *device)
{
	uint8_t request[5] = {QL_ESC, 0x69, 0x64, margins & 0x00FF, (margins & 0xFF00) >> 8};
	fwrite(request, 5, 1, device);
	fflush(device);
}
