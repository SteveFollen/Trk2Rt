// Track to Route Converter - Trk2Rt_RetCode.h
// by Steve Follen, October 2023 - January 2026

#pragma once

#include <stdint.h>


// 32 bit (4 byte) return codes (some retrun codes do not indicate error)
typedef uint32_t tr2RC;

/* upper 2 bits
 *   00 success
 *   01 warning
 *   10 error
 *   11 reserved */

#define T2R_SUCCESS (0)
#define T2R_WARNING (1 << 30)
#define T2R_ERROR   (1 << 31)

#define T2R_IS_SUCCESS(x) (!((x) >> 30))
#define T2R_IS_WARNING(x) (((x) >> 30) & 0b01)
#define T2R_IS_ERROR(x) (((x) >> 31))

// lower 6 bits of upper byte reserved

/* 2nd byte - code module
 * 0x00 reserved
 * 0x01 import
 * 0x02 export
 * 0x03 - 0xFF reserved */

#define T2R_IMPORT (0x01 << 16)
#define T2R_EXPORT (0x02 << 16)

#define T2R_IS_IMPORT(x) ((((X) >> 16) & 0x00FF) == 0x01)
#define T2R_IS_EXPORT(x) ((((X) >> 16) & 0x00FF) == 0x02)

// lower 2 bytes - detail

#define T2R_RC_DETAIL (x)  ((X) & 0x0000FFFF)

typedef enum rcDetail {
	RCD_NO_MEM = 0x00,   // Memory allocation failure
	RCD_PATHNAME,        // File pathname too long
	RCD_FILE_OPEN,       // File open failed
	RCD_TYPE,            // File content is neither gpx nor kml
	RCD_NO_CONTENT,      // File empty or no useful content

	RCD_PARSE_RTPT = 0x10, // Parsing failure - gpx route points
	RCD_PARSE_RTPTEX,      // Parsing failure - gpx route point extensions
	RCD_PARSE_WYPT,        // Parsing failure - gpx waypoints
	RCD_PARSE_TRPT,        // Parsing failure - gpx track points
	RCD_PARSE_KML,         // Parsing failure - kml format invalid
	RCD_PARSE_PMPT,        // Parsing failure - kml placemark points
	RCD_PARSE_LSCRD,       // Parsing failure - kml linestring coorediantes

	RCD_MAX_RTS = 0x20, // Maximum of 99 export route files exceeded
	RCD_NO_SELECT,      // Nothing selected for export
	RCD_NO_MATCH,       // Nothing imported supports selected export

	RCD_REORDER = 0x30, // Imported route or way points reordered to match track

	RCD_MAX = 0xFFFF
} rcDetail_t;
