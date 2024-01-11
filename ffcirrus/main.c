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

#include "ffcirrus.h"
#include "math.h"
#include "./../owt/owt.h"
#include <errno.h>
#include <fcntl.h>

int main(int argc, char** argv)
{
	bool builder = false, g = false, e = false, h = false, u = false;
	bool t = false, p = false, d = false, n = false, b = false, x = false;
	bool r = false, v = false;
	enum ffcirrus_wvfrm_bank bank = INVALID_WVFRM_BANK;
	int duration = 0, gpi = 0, value = 0, mag = DEFAULT_BUZZGEN_MAGNITUDE;
	int fd, i, gain, index, period, ret, x_ms;
	char device_file_name[MAX_NAME_LEN], owt_str[WT_STR_MAX_LEN];
	uint8_t data[WT_TYPE12_PWLE_SINGLE_PACKED_MAX];
	struct ff_effect effect;
	int num_bytes;

	/* Decode Input */
	if (argc <= 1) {
		printf("Missing input arguments\n");
		ffcirrus_display_help();
		return -EINVAL;
	}

	for (i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "-h", 2)) {
			h = true;
		} else if (!strncmp(argv[i], "-e", 2)) {
			strncpy(device_file_name, argv[i + 1], MAX_NAME_LEN);
			e = true;
			i++;
		} else if (!strncmp(argv[i], "-g", 2)) {
			gain = atoi(argv[i + 1]);
			g = true;
			i++;
		} else if(!strncmp(argv[i], "-i", 2)) {
			builder = true;
		} else if (!strncmp(argv[i], "-t", 2)) {
			t = true;
		} else if (!strncmp(argv[i], "-n", 2)) {
			index = atoi(argv[i + 1]);
			n = true;
			i++;
		} else if (!strncmp(argv[i], "-d", 2)) {
			duration = atoi(argv[i + 1]);
			d = true;
			i++;
		} else if (!strncmp(argv[i], "-p", 2)) {
			period = atoi(argv[i + 1]);
			p = true;
			i++;
		} else if (!strncmp(argv[i], "-b", 2)) {
			if (!strncmp(argv[i + 1], "RAM", 3)) {
				bank = RAM_WVFRM_BANK;
			} else if (!strncmp(argv[i + 1], "ROM", 3)) {
				bank = ROM_WVFRM_BANK;
			} else if (!strncmp(argv[i + 1], "BUZ", 3)) {
				bank = BUZ_WVFRM_BANK;
			} else if (!strncmp(argv[i + 1], "OWT", 3)) {
				bank = OWT_WVFRM_BANK;

				strncpy(owt_str, argv[i + 2], WT_STR_MAX_LEN);

				i++;
			} else {
				printf("Invalid bank type, exiting..\n");
				return -EINVAL;
			}

			b = true;
			i++;
		} else if (!strncmp(argv[i], "-x", 2)) {
			x_ms = atoi(argv[i + 1]);
			x = true;
			i++;
		} else if (!strncmp(argv[i], "-u", 2)) {
			u = true;
		} else if (!strncmp(argv[i], "-a", 2)) {
			gpi = atoi(argv[i + 1]);
			i++;
		} else if (!strncmp(argv[i], "-r", 2)) {
			r = true;
		} else if (!strncmp(argv[i], "-m", 2)) {
			mag = atoi(argv[i + 1]);
			i++;
		} else if (!strncmp(argv[i], "-v", 2)) {
			v = true;
		} else {
			printf("Invalid input: %s, continuing..\n", argv[i]);
		}
	}

	if (v)
		owt_version_show();

	if (h)
		ffcirrus_display_help();

	if (!e)
		strncpy(device_file_name, DEFAULT_FILE_NAME, MAX_NAME_LEN);

	/* Open Input Force Feedback Device, path given is an example */
	fd = open(device_file_name, O_RDWR);
	if (fd == -1) {
		printf("Failed to open Input FF device\n");
		return -ENOENT;
	}

	if (g) {
		ret = ffcirrus_set_global_gain(gain, fd);
		if (ret)
			return ret;
	}

	if (t) {
		if (!b) {
			printf("Waveform Bank info. required\n");
			return -EPERM;
		}

		switch (bank) {
		case RAM_WVFRM_BANK:
			if (!n) {
				printf("RAM Waveform requires index\n");
				return -EPERM;
			}

			value = index;
			break;
		case ROM_WVFRM_BANK:
			if (!n) {
				printf("ROM Waveform requires index\n");
				return -EPERM;
			}

			value = index;
			break;
		case BUZ_WVFRM_BANK:
			if (!p || !d) {
				printf("BUZ Waveform requires period and duration\n");
				return -EPERM;
			}

			value = period;
			break;
		case OWT_WVFRM_BANK:
			num_bytes = get_owt_data(owt_str, data);
			if (num_bytes < 0)
				return num_bytes;
			break;
		default:
			printf("Invalid bank type\n");
			return -EINVAL;
		}

		if (value < 0) {
			printf("Index and Period cannot be negative\n");
			return -EINVAL;
		}

		if (bank != OWT_WVFRM_BANK) {
			if (r && bank != BUZ_WVFRM_BANK)
				value *= -1;

			if (ffcirrus_upload_effect(bank, duration, value, gpi, mag, fd, &effect) < 0) {
				printf("Failed to upload effect\n");
				return effect.id;
			}

			printf("effect_id = %d\n", effect.id);

			if (!u) {
				ret = ffcirrus_trigger_effect(effect.id, true, fd);
				if (ret) {
					printf("Failed to trigger effect");
					return ret;
				}
			}
		} else {
			ret = owt_upload(data, num_bytes, gpi, fd, false,
					&effect);
			if (ret < 0) {
				printf("Failed to upload OWT effect\n");
				return ret;
			}
			printf("effect_id = %d\n", effect.id);

			if (!u) {
				ret = owt_trigger(effect.id, fd, true);
				if (ret) {
					printf("Failed to trigger OWT effect\n");
					return ret;
				}
			}
		}

		if (x)
			usleep(x_ms * 1000);
		else
			getchar();
	}

	if (builder) {
		ret = ffcirrus_wavetable_builder(fd);
		if (ret)
			return ret;
	}

	return 0;
}
