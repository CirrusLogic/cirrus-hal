/*
 * Copyright 2021 Cirrus Logic Inc.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <stdint.h>

#define WT_STR_MAX_LEN		512
#define WT_MAX_SEGMENTS		512
#define WT_MAX_SECTIONS		256
#define WT_MAX_DELAY		10000
#define WT_MAX_FINITE_REPEAT	32
#define WT_INDEFINITE		0x00400000
#define WT_LEN_CALCD		0x00800000

#define WT_REPEAT_LOOP_MARKER	0xFF
#define WT_INDEF_TIME_VAL	0xFFFF
#define WT_MAX_TIME_VAL		16383 /* ms */

#define WT_TYPE10_COMP_SEG_LEN_MAX	20

#define WT_TYPE10_COMP_DURATION_FLAG	0x8

#define WT_TYPE12_PWLE_TOTAL_VALS	1787
#define WT_TYPE12_PWLE_MAX_SEG_BYTES	9
#define WT_TYPE12_PWLE_NON_SEG_BYTES	7
#define WT_TYPE12_PWLE_BYTES_MAX	2302
#define WT_TYPE12_PWLE_MAX_RP_VAL		255
#define WT_TYPE12_PWLE_INDEF_TIME_VAL	65535
#define WT_TYPE12_PWLE_MAX_WVFRM_FEAT	12
#define WT_TYPE12_PWLE_WVFRM_FT_SHFT	20
#define WT_TYPE12_PWLE_CHIRP_BIT		0x8
#define WT_TYPE12_PWLE_BRAKE_BIT		0x4
#define WT_TYPE12_PWLE_AMP_REG_BIT	0x2
#define WT_TYPE12_PWLE_SINGLE_PACKED_MAX	1152


/* enums */
enum wt_type12_pwle_specifier {
	PWLE_SPEC_SAVE,
	PWLE_SPEC_FEATURE,
	PWLE_SPEC_REPEAT,
	PWLE_SPEC_WAIT,
	PWLE_SPEC_TIME,
	PWLE_SPEC_LEVEL,
	PWLE_SPEC_FREQ,
	PWLE_SPEC_CHIRP,
	PWLE_SPEC_BRAKE,
	PWLE_SPEC_AR,
	PWLE_SPEC_VBT,
	PWLE_SPEC_INVALID,
};

enum wt_type10_comp_info {
	COMP_INFO_FLAGS_REPEAT,
	COMP_INFO_INDEX,
	COMP_INFO_DELAY,
	COMP_INFO_DURATION,
};

enum wt_type10_comp_specifier {
	COMP_SPEC_OUTER_LOOP,
	COMP_SPEC_INNER_LOOP_START,
	COMP_SPEC_INNER_LOOP_STOP,
	COMP_SPEC_OUTER_LOOP_REPETITION,
	COMP_SPEC_WVFRM,
	COMP_SPEC_DELAY,
	COMP_SPEC_INVALID,
};

/* structs */
struct dspmem_chunk {
	uint8_t *data;
	uint8_t *max;
	int bytes;

	uint32_t cache;
	int cachebits;
};

struct wt_type12_pwle_section {
	uint16_t time;
	uint16_t level;
	uint16_t frequency;
	uint8_t flags;
	uint32_t vbtarget;
};

struct wt_type12_pwle {
	unsigned int feature;
	unsigned int str_len;
	uint32_t wlength;
	uint8_t repeat;
	uint16_t wait;
	uint8_t nsections;
	int fd;

	struct wt_type12_pwle_section sections[WT_MAX_SECTIONS];
};

struct wt_type10_comp_wvfrm {
	uint8_t index;
	uint8_t amplitude;
	uint16_t duration;
};

struct wt_type10_comp_section {
	uint8_t repeat;
	uint8_t flags;
	struct wt_type10_comp_wvfrm wvfrm;
	uint16_t delay;
};

struct wt_type10_comp {
	uint8_t nsections;
	uint8_t repeat;
	bool inner_loop;
	int fd;

	struct wt_type10_comp_section sections[WT_MAX_SECTIONS];
};

/* Function Prototypes */
int get_owt_data(char *, uint8_t *);
