/*
 * ffcirrus.h
 *
 * Userspace program used to interact with Cirrus Logic haptic device drivers
 * making use of the input force feedback subsystem.
 *
 * Copyright 2021 Cirrus Logic, Inc.
 *
 * Author: Fred Treven
 *
 * This file is part of FF Cirrus.
 *
 * FF Cirrus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * FF Cirrus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * You can contact the author by email at this address:
 * Fred Treven <fred.treven@cirrus.com>
 *
 */

#include <linux/input.h>
#include <stdbool.h>

#define CUSTOM_DATA_SIZE	2
#define TEST_CMD_SIZE		4

#define PERIOD_MAX		100 /* 100 ms */
#define PERIOD_MIN		1   /* 1 ms */

#define FFCIRRUS_MAX_EFFECTS	FF_GAIN

#define MAX_NAME_LEN		64
#define NUM_INDV_TESTS		1

#define DEFAULT_FILE_NAME	"/dev/input/event1"

#define RAM_WAVEFORM_BANK_BASE			0x01000000
#define OWT_WAVEFORM_BANK_BASE			0x01400000
#define ROM_WAVEFORM_BANK_BASE			0x01800000

#define BANK_NAME_SIZE		3

#define WVFRM_INDEX_MASK	0x7F
#define WVFRM_BUZZ_SHIFT	7
#define WVFRM_BANK_SHIFT	8
#define WVFRM_GPI_MASK		0x7
#define WVFRM_GPI_SHIFT		12
#define WVFRM_EDGE_SHIFT	15

#define WVFRM_INVERT		0x8000

#define UINT_MAX	0xFFFFFFFF

#define DEFAULT_BUZZGEN_MAGNITUDE	0x50

/* enums */
enum ffcirrus_wvfrm_bank {
	RAM_WVFRM_BANK,
	ROM_WVFRM_BANK,
	BUZ_WVFRM_BANK,
	OWT_WVFRM_BANK,
	INVALID_WVFRM_BANK,
};

enum ffcirrus_cmd {
	CMD_UPLOAD,
	CMD_ERASE,
	CMD_EDIT,
	CMD_TRIGGER,
	CMD_SHOW,
	CMD_GAIN,
	CMD_EXIT
};

enum ffcirrus_waveform_type  {
	FFCIRRUS_WVFRM_SINE,
	FFCIRRUS_WVFRM_CUSTOM
};

/* Private Struct */
struct ffcirrus {
	struct ff_effect effect_list[FFCIRRUS_MAX_EFFECTS];
	unsigned int nuploaded;
	bool running;
	int fd;
	unsigned int nowt;
};

int ffcirrus_upload_effect(enum ffcirrus_wvfrm_bank, int, int, int, int, int, bool,
		struct ff_effect *);
int ffcirrus_trigger_effect(int, bool, int);
int ffcirrus_set_global_gain(int, int);
void ffcirrus_display_help(void);
int ffcirrus_wavetable_builder(int);
