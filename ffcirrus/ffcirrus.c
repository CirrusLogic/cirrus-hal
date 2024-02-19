/*
 * ffcirrus.c
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

#include "ffcirrus.h"
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include "./../owt/owt.h"

static int read_int(const char *prompt, int *val)
{
	char *end;
	char buf[100];
	long int n;
	int ret = -EINVAL;

	printf("%s:", prompt);

	while (fgets(buf, sizeof(buf), stdin)) {
		n = strtol(buf, &end, 10);
		if (end == buf || *end != '\n') { /* No numerical value found */
			printf("Must enter an integer value:");
		} else {
			ret = 0;
			break;
		}
	}

	if (!ret)
		*val = n;

	return ret;
}

static void ffcirrus_upload_prompt(struct ffcirrus *ff)
{
	enum ffcirrus_wvfrm_bank bank = INVALID_WVFRM_BANK;
	int raw_val = -1, duration = 0, magnitude = DEFAULT_BUZZGEN_MAGNITUDE;
	uint8_t data[WT_TYPE12_PWLE_SINGLE_PACKED_MAX];
	char owt_str[WT_STR_MAX_LEN];
	struct ff_effect effect;
	int value, ret;

	if (read_int("Enter Waveform Type (FF_SINE[0] or FF_CUSTOM[1])", &raw_val) < 0) {
		printf("Could not read in value\n");
		return;
	}

	if (raw_val == FFCIRRUS_WVFRM_SINE) {
		bank = BUZ_WVFRM_BANK;
		while(raw_val < PERIOD_MIN || raw_val > PERIOD_MAX) {
			if (read_int("Enter sine wave period in range 1 - 10 ms", &raw_val) < 0) {
				printf("Could not read in value\n");
				return;
			}
		}
		value = raw_val;

		if (read_int("Enter BUZZ magnitude (0 - 255)", &magnitude) < 0) {
			printf("Failed to set magnitude\n");
			return;
		}

	} else if (raw_val == FFCIRRUS_WVFRM_CUSTOM) {
		while (bank == INVALID_WVFRM_BANK) {
			if (read_int("Enter Waveform Bank RAM[0], ROM[1], OWT[3] (BUZ[2] unused here)", &raw_val) < 0) {
				printf("Could not read in value\n");
				return;
			}

			if (raw_val == RAM_WVFRM_BANK)
				bank = RAM_WVFRM_BANK;
			else if (raw_val == ROM_WVFRM_BANK)
				bank = ROM_WVFRM_BANK;
			else if (raw_val == OWT_WVFRM_BANK)
				bank = OWT_WVFRM_BANK;
		}

		if (bank != OWT_WVFRM_BANK) {
			if (read_int("Enter trigger index (offset from bank base)", &raw_val) < 0) {
				printf("Could not read value\n");
				return;
			}
			value = raw_val;
		} else {
			printf("Enter OWT string: ");
			fgets(owt_str, sizeof(owt_str), stdin);

			value = get_owt_data(owt_str, data);
			if (value < 0) {
				printf("Failed to get OWT data\n");
				return;
			}
		}
	} else {
		printf("Invalid waveform type %d\n", raw_val);
		return;
	}

	if (bank != OWT_WVFRM_BANK) {
		if (read_int("Enter playback duration in milliseconds",
				&duration) < 0) {
			printf("Could not read in value\n");
			return;
		}

		ret = ffcirrus_upload_effect(bank, duration, value, 0,
				magnitude, ff->fd, false, &effect);
		if (ret < 0)
			return;
	} else {
		ret = owt_upload(data, value, 0, ff->fd, false, &effect);
		if (ret < 0)
			return;
		else
			ff->nowt++;
	}

	ff->effect_list[ff->nuploaded] = effect;
	ff->nuploaded++;
}

static void ffcirrus_show_effects(struct ffcirrus *ff)
{
	unsigned int full_index, nowt = 0;
	struct ff_effect *effect;
	int16_t offset;
	int i;

	printf("Effect ID \t| Duration (ms) / OWT type | \tTrigger Index \t|");
	printf(" \t\tWaveform Type \t| \t\tPeriod (ms) \t \t|\n");

	for (i = 0; i < ff->nuploaded; i++) {
		effect = &ff->effect_list[i];

		if (effect->u.periodic.waveform == FF_CUSTOM) {
			if (effect->u.periodic.custom_len <= 2) {
				offset = effect->u.periodic.custom_data[1];
				switch (effect->u.periodic.custom_data[0]) {
				case RAM_WVFRM_BANK:
					full_index = RAM_WAVEFORM_BANK_BASE + offset;
					break;
				case ROM_WVFRM_BANK:
					full_index = ROM_WAVEFORM_BANK_BASE + offset;
					break;
				default:
					printf("Invalid waveform bank\n");
					return;
				}
				printf("\t%d \t| \t %d\t \t| \t 0x%08X \t|", effect->id, effect->replay.length, full_index);
				printf("\t \t%s \t| \t \t \t%s \t \t|\n", "FF_CUSTOM", "N/A");
			} else {
				nowt++;
				offset = nowt - 1;
				full_index = OWT_WAVEFORM_BANK_BASE + offset;

				printf("\t%d \t| \t %s\t \t| \t 0x%08X \t|", effect->id, "OWT", full_index);
				printf("\t \t%s \t| \t \t \t%s \t \t|\n", "FF_CUSTOM", "N/A");
			}
		} else {
			printf("\t%d \t| \t %d\t \t| \t \t %s \t|", effect->id, effect->replay.length, "N/A");
			printf("\t \t%s \t| \t \t \t%d \t \t|\n", "FF_SINE", effect->u.periodic.period);
		}
	}
}

static void ffcirrus_get_effect_position(struct ffcirrus *ff, int effect_id, int *pos)
{
	int i;

	*pos = -1;

	if (ff->nuploaded <= 0) {
		printf("No uploaded effects\n");
		return;
	}

	for (i = 0; i < ff->nuploaded; i++) {
		if (ff->effect_list[i].id == effect_id) {
			*pos = i;
			break;
		}
	}

	if (i == ff->nuploaded)
		printf("No such effect [id = %d]\n", effect_id);
}

static void ffcirrus_erase_effect(struct ffcirrus *ff)
{
	int pos, i, effect_id;

	if (read_int("Enter effect ID to remove", &effect_id) < 0) {
		printf("Could not read value\n");
		return;
	}

	ffcirrus_get_effect_position(ff, effect_id, &pos);
	if (pos < 0)
		return;

	fflush(stdout);
	if (ioctl(ff->fd, EVIOCRMFF, effect_id) < 0) {
		printf("Could not remove effect\n");
		return;
	}

	ff->nuploaded--;
	for (i = pos; i < ff->nuploaded; i++) {
		ff->effect_list[i] = ff->effect_list[i + 1];
	}

	printf("Successfully removed effect with ID %d\n", effect_id);
}

static void ffcirrus_trigger_prompt(struct ffcirrus *ff)
{
	int effect_id;

	if (read_int("Enter effect ID to trigger", &effect_id) < 0) {
		printf("Failed to read effect ID\n");
		return;
	}

	ffcirrus_trigger_effect(effect_id, true, ff->fd);
}

static void ffcirrus_edit_effect(struct ffcirrus *ff)
{
	enum ffcirrus_wvfrm_bank bankp, bank = INVALID_WVFRM_BANK;
	int raw_val = -1;
	uint8_t data[WT_TYPE12_PWLE_SINGLE_PACKED_MAX];
	int pos, duration, effect_id, mag, value;
	int indexp, durationp, periodp, magp;
	char owt_str[WT_STR_MAX_LEN];
	struct ff_effect *effect;

	if (read_int("Enter effect ID to edit", &effect_id) < 0) {
		printf("Failed to read ID\n");
		return;
	}

	ffcirrus_get_effect_position(ff, effect_id, &pos);
	if (pos < 0)
		return;

	effect = &ff->effect_list[pos];

	if (effect->u.periodic.waveform == FF_SINE) {
		bank = BUZ_WVFRM_BANK;
		while(raw_val < PERIOD_MIN || raw_val > PERIOD_MAX) {
			if (read_int("Enter sine wave period in range 1 - 10 ms", &raw_val) < 0) {
				printf("Could not read in value\n");
				return;
			}
		}
		periodp = effect->u.periodic.period;
		effect->u.periodic.period = raw_val;

		if (read_int("Enter sine wave magnitude (0-255)", &mag) < 0) {
			printf("Could not get magnitude\n");
			return;
		}
		magp = effect->u.periodic.magnitude;
		effect->u.periodic.magnitude = mag;
	} else {
		bankp = effect->u.periodic.custom_data[0];
		indexp = effect->u.periodic.custom_data[1];

		if (bankp == OWT_WVFRM_BANK) {
			bank = OWT_WVFRM_BANK;
		}

		while (bank == INVALID_WVFRM_BANK) {
			if (read_int("Enter Waveform Bank RAM[0], ROM[1], OWT[3] (BUZ[2] unused here)", &raw_val) < 0) {
				printf("Could not read in value\n");
				return;
			}

			if (raw_val == RAM_WVFRM_BANK)
				bank = RAM_WVFRM_BANK;
			else if (raw_val == ROM_WVFRM_BANK)
				bank = ROM_WVFRM_BANK;
			else if (raw_val == OWT_WVFRM_BANK)
				bank = OWT_WVFRM_BANK;
		}
		effect->u.periodic.custom_data[0] = bank;

		if (bank == OWT_WVFRM_BANK) {
			printf("Enter OWT string: ");
			fgets(owt_str, sizeof(owt_str), stdin);

			value = get_owt_data(owt_str, data);
			if (value < 0) {
				printf("Failed to get OWT data\n");
				return;
			}
		} else {
			if (read_int("Enter trigger index (offset)",
					&raw_val) < 0) {
				printf("Could not read value\n");
				effect->u.periodic.custom_data[0] = bankp;
				return;
			}
		}
		effect->u.periodic.custom_data[1] = raw_val;
	}

	if (read_int("Enter playback duration in milliseconds", &duration)
			< 0)
		printf("Could not read in value\n");

	if (bank == OWT_WVFRM_BANK) {
		if(owt_upload(data, value, 0, ff->fd, true, effect) < 0)
			return;
	}

	durationp = effect->replay.length;
	effect->replay.length = duration;

	if (bank != OWT_WVFRM_BANK) {
		fflush(stdout);
		if(ioctl(ff->fd, EVIOCSFF, effect) == -1) {
			printf("Could not edit waveform\n");

			if (bank != BUZ_WVFRM_BANK) {
				effect->u.periodic.custom_data[0] = bankp;
				effect->u.periodic.custom_data[1] = indexp;
			} else {
				effect->u.periodic.period = periodp;
				effect->u.periodic.magnitude = magp;
			}

			effect->replay.length = durationp;
			return;
		}
	}

	printf("Successfully edited effect with ID %d\n", effect->id);
}

static void ffcirrus_gain_prompt(struct ffcirrus *ff)
{
	int gain = -1;

	while (gain < 0 || gain > 100) {
		if (read_int("Enter intensity as a percentage (0 - 100)", &gain) < 0) {
			printf("Could not read gain\n");
			return;
		}
	}

	ffcirrus_set_global_gain(gain, ff->fd);
}

static void ffcirrus_process_command(struct ffcirrus *ff, enum ffcirrus_cmd cmd)
{
	switch(cmd) {
	case CMD_UPLOAD:
		ffcirrus_upload_prompt(ff);
		break;
	case CMD_ERASE:
		ffcirrus_erase_effect(ff);
		break;
	case CMD_EDIT:
		ffcirrus_edit_effect(ff);
		break;
	case CMD_TRIGGER:
		ffcirrus_trigger_prompt(ff);
		break;
	case CMD_SHOW:
		ffcirrus_show_effects(ff);
		break;
	case CMD_GAIN:
		ffcirrus_gain_prompt(ff);
		break;
	case CMD_EXIT:
		ff->running = false;
		break;
	default:
		printf("Unrecognized command\n");
	}
}

void ffcirrus_display_help(void)
{
	char *tmp;

	printf("Usage: ffcirrus [OPTIONS]...\n\n");

	tmp = "-i";
	printf("\t%s\t", tmp);
	printf("\t%s\t",
	"Launch interactive waveform builder to upload multiple effects\n\n");

	tmp = "-h";
	printf("%s\t", tmp);
	printf("\t%s", "Display this help page and exit\n\n");

	tmp = "-e";
	printf("\t%s\t", tmp);
	printf("\t%s",
	"Path to event device \n \t \t \tDefault: /dev/input/event1\n\n");

	tmp = "-g";
	printf("\t%s\t", tmp);
	printf("\t%s", "Set Global dig. gain (0 - 100%)\n");

	tmp = "-t";
	printf("\t%s\t", tmp);
	printf("\t%s\t", "Perform a one-off trigger\n\n");
	printf("\t%s", "The -t label must be used in conjunction\n");
	printf("\t\t%s", "with the options listed below.\n\n");

	tmp = "-b";
	printf("\t%s\t", tmp);
	printf("\t%s", "Use 'RAM', 'ROM', 'BUZ', or 'OWT' to choose waveform bank\n");
	printf("\t\t\t%s\t", "OWT: create waveforms using Cirrus defined language\n");
	printf("\t\t%s\t", "for composite (type 10) or PWLE (type 12) waveforms\n\n");

	tmp = "-n";
	printf("%s\t", tmp);
	printf("\t%s", "'RAM' and 'ROM' options use the index value to\n");
	printf("\t\t\t%s", "determine the waveform offset. Not used for 'BUZ'\n");

	tmp = "-d";
	printf("\t%s", tmp);
	printf("\t\t%s", "Duration of the waveform in ms\n");

	tmp = "-p";
	printf("\t%s", tmp);
	printf("\t\t%s", "'BUZ' option: set a buzz period between 4 - 10 ms\n");

	tmp = "-m";
	printf("\t%s",  tmp);
	printf("\t\t%s", "Magnitude if 'BUZ' bank selected\n\n\n");

	tmp = "-x";
	printf("\t%s", tmp);
	printf("\t\t%s", "Time in ms after which the program should close\n");

	tmp = "-u";
	printf("\t%s", tmp);
	printf("\t\t%s", "Upload the waveform but do not trigger it\n");

	tmp = "-a";
	printf("\t%s",  tmp);
	printf("\t\t%s", "Set as GPI trigger. Negative value indicates falling edge\n");

	tmp = "-r";
	printf("\t%s",  tmp);
	printf("\t\t%s", "Invert waveform playback\n");

	tmp = "-v";
	printf("\t%s", tmp);
	printf("\t\t%s", "Display OWT library version\n\n\n");

	printf("Examples:\n");
	printf("ffcirrus -i -e /dev/input/event2\n");
	printf("ffcirrus -g 60\n");
	printf("ffcirrus -t -b RAM -n 3 -d 1000\n");
	printf("ffcirrus -t -b ROM -n 2 -x 1000 -d 2000 -u\n");
	printf("ffcirrus -t -b BUZ -p 5 -d 1000 -m 100\n");
	printf("ffcirrus -t -b RAM -n 3 -d 1000 -a -1\n");

	printf("\n\n");

	printf("Detailed documentation can be found at \n");
	printf("<https://docs.cirrus.com/display/SWA/Input+Subsystem+API>\n");
	printf("E-mail bug reports to: <fred.treven@cirrus.com>\n");
}

int ffcirrus_upload_effect(enum ffcirrus_wvfrm_bank bank, int duration,
		int value, int gpi, int magnitude, int fd, bool invert,
		struct ff_effect *effect)
{
	effect->id = -1;
	effect->type = FF_PERIODIC;

	if (bank == BUZ_WVFRM_BANK) {
		effect->u.periodic.waveform = FF_SINE;

		if (value < PERIOD_MIN || value > PERIOD_MAX) {
			printf("Period not in range [1 ms - 10 ms]\n");
			return -EINVAL;
		}
		effect->u.periodic.period = value;
		effect->direction = 0;
	} else if (bank == RAM_WVFRM_BANK || bank == ROM_WVFRM_BANK) {
		effect->u.periodic.waveform = FF_CUSTOM;

		effect->u.periodic.custom_data =
			(int16_t*) malloc(sizeof(int16_t) * CUSTOM_DATA_SIZE);
		if (effect->u.periodic.custom_data == NULL) {
			printf("Failed to allocate memory for custom data\n");
			return -ENOMEM;
		}

		effect->u.periodic.custom_data[0] = bank;
		effect->u.periodic.custom_data[1] = value;
		if (invert)
			effect->direction = WVFRM_INVERT;
		else
			effect->direction = 0;

		effect->u.periodic.custom_len = CUSTOM_DATA_SIZE;
	} else if (bank == OWT_WVFRM_BANK) {
		//Do some stuff
		return -1;
	} else {
		printf("Invalid waveform type %u selected\n", bank);
		return -EINVAL;
	}

	effect->replay.length = duration;

	if (gpi) {
		effect->trigger.button = gpi_config((gpi >= 0), abs(gpi));
		if (effect->trigger.button)
			printf("Button config = 0x%04X\n",
							effect->trigger.button);
	} else {
		effect->trigger.button = 0;
	}

	/* This only affects FF_SINE effects */
	effect->u.periodic.magnitude = magnitude;

	fflush(stdout);
	if(ioctl(fd, EVIOCSFF, effect) == -1) {
			printf("Could not upload waveform\n");
			goto exit_free;
	}

	printf("Successfully uploaded effect with ID = %d\n", effect->id);
	return effect->id;

exit_free:
	if (effect->u.periodic.waveform == FF_CUSTOM)
		free(effect->u.periodic.custom_data);

	return effect->id;
}

int ffcirrus_trigger_effect(int effect_id, bool play, int fd) {
	struct input_event event;

	memset(&event, 0, sizeof(event));
	event.type = EV_FF;
	event.value = play ? 1 : 0;
	event.code = effect_id;

	fflush(stdout);
	if ((write(fd, (const void*) &event, sizeof(event))) == -1 ) {
		printf("Could not play effect\n");
		return -ENXIO;
	}

	return 0;
}

int ffcirrus_set_global_gain(int gain, int fd)
{
	struct input_event event;

	event.type = EV_FF;
	event.code = FF_GAIN;
	event.value = gain;

	printf("Setting master gain to %u percent \n", event.value);

	fflush(stdout);
	if (write(fd, &event, sizeof(event)) != sizeof(event)) {
		printf("Failed to set master gain\n");
		return -ENXIO;
	}

	return 0;
}

int ffcirrus_wavetable_builder(int fd)
{
	struct ffcirrus ff;
	int cmd, i;

	ff.nuploaded = 0;
	ff.nowt = 0;
	ff.running = true;
	ff.fd = fd;

	printf("Interactive wavetable builder\n\n");
	printf("This function allows the user to dynamically upload,");
	printf("remove, edit, and trigger FF effects\n\n");

	while (ff.running) {
		printf("Choose between commands:\n upload[0]\n erase[1]\n");
		printf(" edit [2]\n trigger[3]\n show[4]\n gain[5]\n exit[6]\n\n");

		if (read_int("Enter Command", &cmd) < 0)
			printf("Could not read in value\n");
		else
			ffcirrus_process_command(&ff, cmd);

		printf("\n");
	}

	for (i = 0; i < ff.nuploaded; i++) {
		if (ff.effect_list[i].u.periodic.waveform == FF_CUSTOM)
			free(ff.effect_list[i].u.periodic.custom_data);
	}

	return 0;
}
