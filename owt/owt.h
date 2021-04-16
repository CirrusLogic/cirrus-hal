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

#define WT_TYPE12_PWLE_WT_NUM_GPIO_VSLOTS	2
#define WT_TYPE12_PWLE_WT_NUM_COMP_VSLOTS	1

#define WT_TYPE12_PWLE_WT_NUM_VIRT_SLOTS	(\
	WT_TYPE12_PWLE_WT_NUM_COMP_VSLOTS +\
	WT_TYPE12_PWLE_WT_NUM_GPIO_VSLOTS)

#define WT_TYPE12_PWLE_MIN_SEGS		2
#define WT_TYPE12_PWLE_MAX_SEGS		255
#define WT_TYPE12_PWLE_MAX_SEG_VALS	7
#define WT_TYPE12_PWLE_MAX_TOT_SV (\
	WT_TYPE12_PWLE_MAX_SEGS *\
	WT_TYPE12_PWLE_MAX_SEG_VALS)
#define WT_TYPE12_PWLE_TOTAL_VALS	(\
	WT_TYPE12_PWLE_MAX_TOT_SV +\
	WT_TYPE12_PWLE_MIN_SEGS)
#define WT_TYPE12_PWLE_MAX_SEG_BYTES	9
#define WT_TYPE12_PWLE_NON_SEG_BYTES	7
#define WT_TYPE12_PWLE_BYTES_MAX	((\
	WT_TYPE12_PWLE_MAX_SEGS *\
	WT_TYPE12_PWLE_MAX_SEG_BYTES) +\
	WT_TYPE12_PWLE_NON_SEG_BYTES)
#define WT_TYPE12_PWLE_SEG_LEN_MAX	11
#define WT_TYPE12_PWLE_MAX_RP_VAL		255
#define WT_TYPE12_PWLE_MAX_WT_VAL		1023
#define WT_TYPE12_PWLE_MAX_TIME_VAL	16383
#define WT_TYPE12_PWLE_INDEF_TIME_VAL	65535
#define WT_TYPE12_PWLE_TIME_RES		25
#define WT_TYPE12_PWLE_FREQ_RES		125
#define WT_TYPE12_PWLE_MAX_LEV_VAL	98256
#define WT_TYPE12_PWLE_LEV_ADD_NEG	2048
#define WT_TYPE12_PWLE_LEV_DIV		48
#define WT_TYPE12_PWLE_MAX_FREQ_VAL	561
#define WT_TYPE12_PWLE_MIN_FREQ_VAL	50
#define WT_TYPE12_PWLE_MAX_VB_RES		9999999
#define WT_TYPE12_PWLE_MAX_VB_TARG	8388607
#define WT_TYPE12_PWLE_NUM_CONST_VALS	2
#define WT_TYPE12_PWLE_MAX_VB_RES_DIG	6
#define WT_TYPE12_PWLE_MAX_LV_RES_DIG	4
#define WT_TYPE12_PWLE_MAX_WVFRM_FEAT	12
#define WT_TYPE12_PWLE_WVFRM_FT_SHFT	20
#define WT_TYPE12_PWLE_SAMPLES_PER_MS	8
#define WT_TYPE12_PWLE_SEG_BYTES		6
#define WT_TYPE12_PWLE_WV_SMPL_BYTES	3
#define WT_TYPE12_PWLE_REPEAT_BYTES	1
#define WT_TYPE12_PWLE_WT_BYTES		2
#define WT_TYPE12_PWLE_NUM_SEG_BYTES	1
#define WT_TYPE12_PWLE_NUM_VBT_BYTES	3
#define WT_TYPE12_PWLE_END_PAD_BYTES	2
#define WT_TYPE12_PWLE_ZERO_PAD_MASK		0xFFFFFF00
#define WT_TYPE12_PWLE_MS_FOUR_BYTE_MASK	0xF0
#define WT_TYPE12_PWLE_LS_FOUR_BYTE_MASK	0x0F
#define WT_TYPE12_PWLE_CHIRP_BIT		0x8
#define WT_TYPE12_PWLE_BRAKE_BIT		0x4
#define WT_TYPE12_PWLE_AMP_REG_BIT	0x2
#define WT_TYPE12_PWLE_FIRST_BYTES	(\
	WT_TYPE12_PWLE_WV_SMPL_BYTES +\
	WT_TYPE12_PWLE_REPEAT_BYTES +\
	WT_TYPE12_PWLE_WT_BYTES +\
	WT_TYPE12_PWLE_NUM_SEG_BYTES)
#define WT_TYPE12_PWLE_PACKED_BYTES_MAX	(((\
	WT_TYPE12_PWLE_BYTES_MAX / 2) *\
	WT_TYPE12_PWLE_WT_NUM_VIRT_SLOTS) + 3)
#define WT_TYPE12_PWLE_SINGLE_PACKED_MAX	(\
	WT_TYPE12_PWLE_PACKED_BYTES_MAX /\
	WT_TYPE12_PWLE_WT_NUM_VIRT_SLOTS)


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

enum wt_type10_comp_segment_type {
	COMP_SEGMENT_TYPE_WVFRM,
	COMP_SEGMENT_TYPE_DELAY,
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

struct wt_type10_comp_segment {
	uint8_t repeat;
	uint8_t flags;
	enum wt_type10_comp_segment_type type;

	union {
		struct wt_type10_comp_wvfrm wvfrm;
		uint16_t delay;
	} params;
};

struct wt_type10_comp_section {
	uint8_t repeat;
	uint8_t flags;
	bool has_delay;
	bool has_wvfrm;
	uint16_t delay;
	struct wt_type10_comp_wvfrm wvfrm;
};

struct wt_type10_comp {
	uint8_t nsegments;
	uint8_t nsections;
	uint8_t repeat;
	bool inner_loop;
	int fd;

	struct wt_type10_comp_segment segments[WT_MAX_SEGMENTS];
	struct wt_type10_comp_section sections[WT_MAX_SECTIONS];
};

/* Function Prototypes */
int get_owt_data(char *, uint8_t *);
