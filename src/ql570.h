/* ql570.h: facilities for communicating with the QL-570 label printer
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

#ifndef _QL570_H_
#define _QL570_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define QL_ESC 0x1b
#define QL_INVALID 0x00

enum ql_extended_option {
	OPT_CUT_AT_END      = 0x08,
	OPT_HIGH_RESOLUTION = 0x40
};

enum ql_printer_type {
	QL_OTHER   = 0x00,
	QL_500_550 = 0x4F,
	QL_560     = 0x31,
	QL_570     = 0x32,
	QL_580N    = 0x33,
	QL_650TD   = 0x51,
	QL_700     = 0x35,
	QL_1050    = 0x50,
	QL_1060N   = 0x34
};

enum ql_error_info_1 {
	NO_MEDIA         = 0x01,
	END_OF_MEDIA     = 0x02,
	TAPE_CUTTER_JAM  = 0x04,
	MAIN_UNIT_IN_USE = 0x10,
	FAN_MALFUNCTION  = 0x80
};

enum ql_error_info_2 {
	/**
	 * Requested media type is not loaded into the printer.
	 *
	 * Another gem from the specification. The table in section 4.2.1
	 * describes that bit 0 of _Error information 2_ is unused.  The
	 * command description for _Print information command_ states that bit
	 * 0 of that field is set to `true` when media type, width and length
	 * are set in the struct's `valid_flag` and the wrong media is loaded.
	 */
	WRONG_MEDIA	   = 0x01,
	TRANSMISSION_ERROR = 0x04,
	COVER_OPENED       = 0x10,
	CANNOT_FEED        = 0x40,
	SYSTEM_ERROR       = 0x80
};

enum ql_media_type {
	MT_CONTINUOUS = 0x0A,
	MT_DIE_CUT    = 0x0B
};

enum ql_status_type {
	ST_REPLY        = 0x00,
	ST_COMPLETED    = 0x01,
	ST_ERROR        = 0x02,
	ST_NOTIFICATION = 0x05,
	ST_PHASE_CHANGE = 0x06
};

enum ql_notification_type {
	NT_NA = 0x00,
	NT_COOLING_STARTED  = 0x03,
	NT_COOLING_FINISHED = 0x04
};

enum ql_phase_type {
	PT_WAITING  = 0x00,
	PT_PRINTING = 0x01
};

enum ql_print_info_validity {
	/**
	 * Stop with error (`ql_error_info_2.WRONG_MEDIA`) if the requested
	 * media type is different from the loaded media type.
	 */
	PIV_MEDIA_TYPE    = 0x02,

	/**
	 * Stop with error (`ql_error_info_2.WRONG_MEDIA`) if the requested
	 * media width is different from the loaded media width.
	 */
	PIV_MEDIA_WIDTH   = 0x04,

	/**
	 * Stop with error (`ql_error_info_2.WRONG_MEDIA`) if the requested
	 * media length is different from the loaded media length.
	 */
	PIV_MEDIA_LENGTH  = 0x08,

	/**
	 * Prefer quality over speed.
	 */
	PIV_QUALITY       = 0x40,

	/**
	 * A mysterious flag described as "Always ON" in the specification.  An
	 * initial suspicion that this might signal the printer to recover from
	 * errors after a short amount of time turned out to be wrong. (This
	 * was based on the name `PI_RECOVER` as used in the specification.)
	 */
	PIV_RECOVER       = 0x80
};

typedef struct ql_print_info ql_print_info;
struct ql_print_info {
	/*
	 * See #ql_print_info_validity.
	 */
	uint8_t valid_flag;
	uint8_t media_type;
	uint8_t media_width;
	uint8_t media_length;

	/*
	 * Number of lines to be printed.
	 *
	 * TODO: rename?
	 */
	uint8_t raster_number[4];

	/**
	 * TODO: What are the effects of this setting?
	 *
	 * Set to `0` on the first page, `1` for successive pages;
	 */
	uint8_t successive_page;
	uint8_t _fixed;
};

typedef struct ql_status ql_status;
struct ql_status {
	/**
	 * Always 0x80.
	 */
	uint8_t print_head_mark;

	/**
	 * Always 32 bytes.
	 */
	uint8_t size;
	uint8_t _reserved3;
	uint8_t _reserved4;

	/**
	 * Officially named 'reserved' but according to the documentation it
	 * indeed does identify the printer. Note that the specification is
	 * from 2011 and thus cannot include any newer printers in the QL
	 * series.
	 */
	uint8_t printer_id;

	/**
	 * Always 0x30.
	 */
	uint8_t _reserved6;

	/**
	 * Always 0x00.
	 */
	uint8_t _reserved7;

	/**
	 * Always 0x00.
	 */
	uint8_t _reserved8;

	/**
	 * See #ql_error_info_1.
	 */
	uint8_t error_info_1;

	/**
	 * See #ql_error_info_2.
	 */
	uint8_t error_info_2;

	/**
	 * Width of the label.
	 */
	uint8_t media_width;

	/**
	 * See #ql_media_type.
	 */
	uint8_t media_type;

	/**
	 * Always 0x00.
	 */
	uint8_t _reserved13;

	/**
	 * Always 0x00.
	 */
	uint8_t _reserved14;

	/**
	 * Not set.
	 */
	uint8_t _reserved15;

	/**
	 * Not set.
	 */
	uint8_t _reserved16;

	/**
	 * Always 0x00.
	 */
	uint8_t _reserved17;

	/**
	 * Length of ready-to-use labels, or zero for continuous labels.
	 */
	uint8_t media_length;

	/**
	 * See #ql_status_type.
	 */
	uint8_t status_type;

	/**
	 * See #ql_phase_type.
	 */
	uint8_t phase_type;

	/**
	 * Effectively 0x00. The specification states that this fields is 0x00
	 * if it is not used (i.e. no 'phase change' was indicated in the
	 * status_type field.). The table for the 'phase types' also indicates
	 * that this field is 0x00 in both phases.
	 */
	uint8_t phase_num_h;

	/**
	 * Effectively 0x00. The specification states that this fields is 0x00
	 * if it is not used (i.e. no 'phase change' was indicated in the
	 * status_type field.). The table for the 'phase types' also indicates
	 * that this field is 0x00 in both phases.
	 */
	uint8_t phase_num_l;

	/**
	 * See #ql_notification_type.
	 */
	uint8_t notification_type;

	/**
	 * Not set.
	 */
	uint8_t _reserved24;

	/**
	 * Not set. This is probably just for padding the struct to 32 bytes.
	 */
	uint8_t _reserved25[8];
};

void ql_init(bool flush, FILE* device);
void ql_status_request(FILE* device);
bool ql_status_read(ql_status* status, FILE* device);
void ql_status_debug(ql_status* status);
void ql_raster(uint8_t length, uint8_t* data, FILE* device);
void ql_raster_end(uint8_t length, FILE* device);
void ql_page_start(ql_print_info* print_info, FILE* device);
void ql_page_end(bool last_page, FILE* device);
void ql_set_extended_options(bool cutAtEnd, bool highResolution, FILE* device);
void ql_autocut_enable(FILE* device);
void ql_autocut_interval(uint8_t interval, FILE* device);
void ql_set_default_margins(enum ql_media_type, FILE* device);
void ql_set_margins(uint16_t margins, FILE* device);

#endif
