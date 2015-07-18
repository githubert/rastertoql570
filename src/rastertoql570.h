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

#ifndef _RASTERTOQL570_H
#define _RASTERTOQL570_H

bool backchannel_read_status(ql_status*);
bool init(ql_status*, FILE*);
bool request_status(ql_status*, FILE*);
void wait_for_page_end();
bool handle_status(ql_status*);
void print_blank_lines(uint32_t count, size_t buffer_size, FILE *device);
void handle_page(cups_raster_t*, cups_page_header2_t, ql_status, unsigned int, FILE*);

#endif
