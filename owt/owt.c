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

#include <errno.h>
#include <math.h>
#include "owt.h"

/***********/
/* General */
/***********/

/*
 * strnchr() - Find character in a given string
 *
 * @s: String to search
 * @count: Number of characters to search
 * @c: Character being searched for
 *
 * Parses @s looking for @c for @count elements or until the end of @s.
 *
 * Returns the character being searched for or NULL if not found.
 *
 */

static char *strnchr(const char *s, unsigned int count, int c)
{
	while (count--) {
		if (*s == (char) c)
			return (char *) s;
		if (*s++ == '\0')
			break;
	}
	return NULL;
}

/*
 * min() - Find smaller of two integers
 *
 * @x: First integer to compare
 * @y: Second integer to compare
 *
 * Returns smaller value between @x and @y.
 *
 */
static int min(int x, int y) {
	return x < y ? x : y;
}

/*
 * parse_float() - Convert floating point value to integer
 *
 * @frac - Floating point value to be converted
 * @result - Resulting integer
 * @scale - Multiplier to increase the result by to obtain an integer
 * @minimum - Minimum allowable value for @result
 * @maximum - Maximum allowable value for @result
 *
 * Returns 0 upon success.
 * Returns negative errno upon error condition.
 *
 */
static int parse_float(char *frac, int *result, int scale, int min, int max)
{
	float fres;

	errno = 0;
	fres = strtof(frac, NULL);
	if (errno)
		return -errno;

	*result = roundf(fres * scale);

	if (*result < min || *result >  max)
		return -ERANGE;

	return 0;
}

/************************/
/* Memchunk Formatting */
/************************/

/*
 * dspmem_chunk_create() - Create dspmem_chunk struct for given buffer
 *
 * @data: Buffer to be associated with dspmem_chunk struct
 * @size: Size of data buffer in bytes
 *
 * Returns dspmem_chunk struct whose data pointer is the same as the provided
 * @data buffer with a max value at the expected size.
 *
 */
static struct dspmem_chunk dspmem_chunk_create(void *data, int size)
{
	struct dspmem_chunk ch = {
		.data = data,
		.max = data + size,
	};

	return ch;
}

/*
 * dspmem_chunk_end() - Check if dspmem_chunk struct is full
 *
 * @ch: Pointer to dspmem_chunk struct
 *
 * Returns true if @ch's data buffer is full.
 * Returns false if @ch's data buffer is not full.
 *
 */
static bool dspmem_chunk_end(struct dspmem_chunk *ch) {
	return ch->data == ch->max;
}

/*
 * dspmem_chunk_bytes() - Get number of bytes in dspmem_chunk struct data
 *
 * @ch: Pointer to dspmem_chunk struct
 *
 * Returns number of bytes in @ch's data field.
 *
 */
static int dspmem_chunk_bytes(struct dspmem_chunk *ch)
{
	return ch->bytes;
}

/*
 * dspmem_chunk_write() - Write data to dspmem_chunk struct
 *
 * @ch: Pointer to dspmem_chunk struct
 * @nbits: Number of bits to be written
 * @val: Value to be written
 *
 * Returns 0.
 *
 */
static int dspmem_chunk_write(struct dspmem_chunk *ch, int nbits, uint32_t val)
{
	int nwrite, i;

	nwrite = min(24 - ch->cachebits, nbits);

	ch->cache <<= nwrite;
	ch->cache |= val >> (nbits - nwrite);
	ch->cachebits += nwrite;
	nbits -= nwrite;

	if (ch->cachebits == 24) {
		if (dspmem_chunk_end(ch))
			return -ENOSPC;

		ch->cache &= 0xFFFFFF;
		for (i = 0; i < sizeof(ch->cache); i++, ch->cache <<= 8)
			*ch->data++ = (ch->cache & 0xFF000000) >> 24;

		ch->bytes += sizeof(ch->cache);
		ch->cachebits = 0;
	}

	if (nbits)
		return dspmem_chunk_write(ch, nbits, val);

	return 0;
}

/*
 * dspmem_chunk_flush() - Clear data from given memchunk's cache
 *
 * @ch: Pointer to dspmem_chunk struct
 *
 * Returns 0.
 *
 */
static int dspmem_chunk_flush(struct dspmem_chunk *ch)
{
	if (!ch->cachebits)
		return 0;

	return dspmem_chunk_write(ch, 24 - ch->cachebits, 0);
}

/*******************************/
/* Waveform Type 10: Composite */
/*******************************/

/*
 * wt_type10_comp_to_buffer() - Format data to be written directly to wavetable
 *
 * @wave: Pointer to the composite struct
 * @size: Size of @buf in bytes
 * @buf: Data buffer to be written to
 *
 * Use dspmem_chunk() functions to format data from sections into structure
 * expected by the device. This buffer represents the data that will be written
 * directly to the RAM wavetable.
 *
 * Returns the total number of bytes written to the buffer. Note that this is
 * different than the buffer size.
 *
 */

static int wt_type10_comp_to_buffer(struct wt_type10_comp *comp, int size,
		void *buf)
{
	struct dspmem_chunk ch = dspmem_chunk_create(buf, size);
	int i;

	dspmem_chunk_write(&ch, 8, 0); /* Padding */
	dspmem_chunk_write(&ch, 8, comp->nsections);
	dspmem_chunk_write(&ch, 8, comp->repeat);

	for (i = 0; i < comp->nsections; i++) {
		dspmem_chunk_write(&ch, 8, comp->sections[i].wvfrm.amplitude);
		dspmem_chunk_write(&ch, 8, comp->sections[i].wvfrm.index);
		dspmem_chunk_write(&ch, 8, comp->sections[i].repeat);
		dspmem_chunk_write(&ch, 8, comp->sections[i].flags);
		dspmem_chunk_write(&ch, 16, comp->sections[i].delay);

		if (comp->sections[i].flags & WT_TYPE10_COMP_DURATION_FLAG) {
			dspmem_chunk_write(&ch, 8, 0); /* Padding */
			dspmem_chunk_write(&ch, 16,
					comp->sections[i].wvfrm.duration);
		}
	}

	return dspmem_chunk_bytes(&ch);
}

/*
 * wt_type10_comp_specifier_get() - Retrieve symbol specifier from given string
 *
 * @str: The string to be analyzed, a segment of the larger composite string
 *
 * Retrieve the specifier enum value that corresponds to the given string.
 * This is used to determine which action should be taken when parsing.
 *
 * Returns the enum value of the associated string; COMP_SPEC_INVALID is
 * returned if the string does not match any expected case.
 *
 */
static enum wt_type10_comp_specifier wt_type10_comp_specifier_get(char *str)
{
	if (!strcmp(str, "~"))
		return COMP_SPEC_OUTER_LOOP;
	else if (!strcmp(str, "!!"))
		return COMP_SPEC_INNER_LOOP_START;
	else if (strstr(str, "!!"))
		return COMP_SPEC_INNER_LOOP_STOP;
	else if (strnchr(str, WT_TYPE10_COMP_SEG_LEN_MAX, '!'))
		return COMP_SPEC_OUTER_LOOP_REPETITION;
	else if (strnchr(str, WT_TYPE10_COMP_SEG_LEN_MAX, '.'))
		return COMP_SPEC_WVFRM;
	else
		return COMP_SPEC_DELAY;

	return COMP_SPEC_INVALID;
}

/*
 * wt_type10_comp_waveform_get() - Retrieve waveform information from string
 *
 * @str: String containing waveform information
 * @wave: Pointer to the waveform structure to be populated
 *
 * Gets index, amplitude, and duration (if applicable) for
 * COMP_SEGMENT_TYPE_WVFRM segments.
 *
 * A value of 0 will be returned on success; a negative errno will be returned
 * in error cases.
 *
 */
static int wt_type10_comp_waveform_get(char *str,
				       struct wt_type10_comp_wvfrm *wave) {
	unsigned int index_tmp, amp_tmp, duration_tmp;
	int ret;

	ret = sscanf(str, "%u.%u.%u", &index_tmp, &amp_tmp, &duration_tmp);
	if (ret < 2) {
		printf("Failed to parse waveform\n");
		return -EINVAL;
	}

	if (ret == 2 && index_tmp == 0) {
		printf("Invalid waveform index: %u\n", index_tmp);
		return -EINVAL;
	}

	if (amp_tmp == 0 || amp_tmp > 100) {
		printf("Invalid waveform amplitude: %u\n", amp_tmp);
		return -EINVAL;
	}

	if (ret == 3) { /* All data processed */
		if (duration_tmp != WT_INDEF_TIME_VAL) {
			if (duration_tmp > WT_MAX_TIME_VAL) {
				printf("Duration too long: %u ms\n", duration_tmp);
				return -EINVAL;
			}
			duration_tmp *= 4;
		}
	} else {
		duration_tmp = 0;
	}

	wave->index = (uint8_t) (0xFF & index_tmp);
	wave->amplitude = (uint8_t) (0xFF & amp_tmp);
	wave->duration = (uint16_t) (0xFFFF & duration_tmp);

	return 0;
}

/*
 * wt_type10_comp_decode() - Define segments according to given specifier
 *
 * @comp: Pointer to the composite waveform type struct
 * @specifier: Symbol to decode
 * @str: Value associated with the given specifier. i.e. Number of repeats
 * for COMP_SPEC_INNER_LOOP_STOP
 *
 * Populates the segment struct array which will be used to format binary
 * information. This is based on the current symbol as well as its relation
 * to the rest of the sequence.
 *
 * A value of 0 will be returned on success; a negative errno will be returned
 * in error cases.
 *
 */
static int wt_type10_comp_decode(struct wt_type10_comp *comp,
				 enum wt_type10_comp_specifier specifier,
				 char *str)
{
	struct wt_type10_comp_section *section;
	unsigned long l_val;
	int ret;

	section = &comp->sections[comp->nsections];

	switch (specifier) {
	case COMP_SPEC_OUTER_LOOP:
		if (comp->repeat) {
			printf("Duplicate outer loop specifier\n");
			return -EINVAL;
		}

		comp->repeat = WT_REPEAT_LOOP_MARKER;
		break;
	case COMP_SPEC_INNER_LOOP_START:
		if (comp->inner_loop) {
			printf("Nested inner loop specifier not allowed\n");
			return -EINVAL;
		}

		if (section->wvfrm.amplitude || section->delay) {
			section++;
			comp->nsections++;
		}

		section->repeat = WT_REPEAT_LOOP_MARKER;

		comp->inner_loop = true;
		break;
	case COMP_SPEC_INNER_LOOP_STOP:
		if (!comp->inner_loop) {
			printf("Error: Inner loop stop with no start\n");
			return -EINVAL;
		}
		comp->inner_loop = false;

		l_val = strtoul(strsep(&str, "!"), NULL, 10);
		if (!l_val) {
			printf("Failed to get inner loop repeat value\n");
			return -EINVAL;
		}

		section->repeat = (uint8_t) (0xFF & l_val);

		section++;
		comp->nsections++;
		break;
	case COMP_SPEC_OUTER_LOOP_REPETITION:
		if (comp->repeat) {
			printf("Duplicate outer loop specifier\n");
			return -EINVAL;
		}

		l_val = strtoul(strsep(&str, "!"), NULL, 10);
		if (!l_val) {
			printf("Failed to get outer loop specifier\n");
			return -EINVAL;
		}

		comp->repeat = (uint8_t) (0xFF & l_val);
		break;
	case COMP_SPEC_WVFRM:
		if (section->wvfrm.amplitude || section->delay) {
			section++;
			comp->nsections++;
		}

		ret = wt_type10_comp_waveform_get(str, &section->wvfrm);
		if (ret)
			return ret;

		if (section->wvfrm.duration != 0)
			section->flags |= WT_TYPE10_COMP_DURATION_FLAG;
		break;
	case COMP_SPEC_DELAY:
		if (section->delay) {
			section++;
			comp->nsections++;
		}

		l_val = strtoul(str, NULL, 10);
		if (!l_val) {
			printf("Failed to get inner loop repeat value\n");
			return -EINVAL;
		}

		if (l_val > WT_MAX_DELAY) {
			printf("Delay %lu too long\n", l_val);
			return -EINVAL;
		}
		section->delay = (uint16_t) (0xFFFF & l_val);
		break;
	default:
		printf("Invalid specifier\n");
		return -EINVAL;
	}

	return 0;
}

/*
 * wt_type10_comp_str_to_bin() - Converts Composite string to binary data
 *
 * @full_str: Entirety of the type 10 (Composite) OWT string
 * @data: Data buffer to be written to
 *
 * Converts the string to binary data that can be written directly to the
 * device for Composite Waveforms.
 *
 * Returns total number of bytes in @data upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type10_comp_str_to_bin(char *full_str, uint8_t *data)
{
	enum wt_type10_comp_specifier specifier;
	char delim[] = ", \n";
	struct wt_type10_comp comp = { 0 };
	char *str;
	int ret;

	str = strtok(full_str, delim);
	while (str != NULL) {
		specifier = wt_type10_comp_specifier_get(str);
		ret = wt_type10_comp_decode(&comp, specifier, str);
		if (ret)
			return ret;

		str = strtok(NULL, delim);
	}

	if (comp.inner_loop) {
		printf("Inner loop never terminated\n");
		return -EINVAL;
	}

	if (comp.sections[comp.nsections].wvfrm.amplitude ||
	    comp.sections[comp.nsections].delay)
		comp.nsections++;

	return wt_type10_comp_to_buffer(&comp,
			WT_TYPE12_PWLE_SINGLE_PACKED_MAX, data);
}

/**************************/
/* Waveform Type 12: PWLE */
/**************************/

/*
 * wt_type12_pwle_specifier_get() - Retrieve parameter type from string
 *
 * @str: String to be analyzed
 *
 * Returns the type of parameter being described in the provided string.
 *
 * Returns enum value for specifier.
 * Returns PWLE_SPEC_INVALID if no specifier is found in @str
 *
 */
static enum wt_type12_pwle_specifier wt_type12_pwle_specifier_get(char *str)
{
	if (str[0] == 'S')
		return PWLE_SPEC_SAVE;
	else if (!strncmp(str, "WF", 2))
		return PWLE_SPEC_FEATURE;
	else if (!strncmp(str, "RP", 2))
		return PWLE_SPEC_REPEAT;
	else if (!strncmp(str, "WT", 2))
		return PWLE_SPEC_WAIT;
	else if (str[0] == 'T')
		return PWLE_SPEC_TIME;
	else if (str[0] == 'L')
		return PWLE_SPEC_LEVEL;
	else if (str[0] == 'F')
		return PWLE_SPEC_FREQ;
	else if (str[0] == 'C')
		return PWLE_SPEC_CHIRP;
	else if (str[0] == 'B')
		return PWLE_SPEC_BRAKE;
	else if (!strncmp(str, "AR", 2))
		return PWLE_SPEC_AR;
	else if (str[0] == 'V')
		return PWLE_SPEC_VBT;
	else
		return PWLE_SPEC_INVALID;
}

/*
 * wt_type12_pwle_save_entry() - Get value for save (1 or 0)
 *
 * @token: Portion of string being analyzed
 *
 * Verifies if the save value is valid. This value is unused but required
 * to ensure that the string is formatted properly.
 *
 * Returns 0 upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type12_pwle_save_entry(char *token)
{
	int val = atoi(token);

	if (val == 1 || val == 0) {
		return 0;
	} else {
		printf("Save value must be 0 or 1\n");
		return -EINVAL;
	}
}

/*
 * wt_type12_pwle_feature_entry() - Get feature specification from string
 *
 * @pwle: Pointer to struct with PWLE information
 * @token: Portion of string being analyzed
 *
 * Get the feature information from the string.
 *
 * Returns 0 upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type12_pwle_feature_entry(struct wt_type12_pwle *pwle,
		char *token)
{
	int val = atoi(token);

	if (val > WT_TYPE12_PWLE_MAX_WVFRM_FEAT || (val % 4) != 0 || val < 0) {
		printf("Valid waveform features are: 0, 4, 8, 12\n");
		return -EINVAL;
	}

	pwle->feature = val << WT_TYPE12_PWLE_WVFRM_FT_SHFT;

	return 0;
}

/*
 * wt_type12_pwle_repeat_entry() - Get repeat value from string
 *
 * @pwle: Pointer to struct with PWLE information
 * @token: Portion of string being analyzed
 *
 * Get the global repeat value from the PWLE string.
 *
 * Returns 0 upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type12_pwle_repeat_entry(struct wt_type12_pwle *pwle, char *token)
{
	int val = atoi(token);

	if (val < 0 || val > WT_TYPE12_PWLE_MAX_RP_VAL) {
		printf("Valid repeat: 0 to 255\n");
		return -EINVAL;
	}

	pwle->repeat = val;

	return 0;
}

/*
 * wt_type12_pwle_wait_time_entry() - Get wait time value from string
 *
 * @pwle: Pointer to struct with PWLE information
 * @token: Portion of string being analyzed
 *
 * Converts the wait time floating point value provided to an integer
 * that can be interpreted by the device driver.
 *
 * Returns 0 upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type12_pwle_wait_time_entry(struct wt_type12_pwle *pwle,
		char *token)
{
	int ret, val;

	/* Valid values from spec.: 0 ms - 1023.75 ms */
	ret = parse_float(token, &val, 100, 0, 102375);
	if (ret) {
		printf("Failed to parse wait time: %d\n", ret);
		return ret;
	}

	pwle->wait = val / (100 / 4);
	pwle->wlength += pwle->wait;

	return 0;
}

/*
 * wt_type12_pwle_time_entry() - Get time value from string
 *
 * @pwle: Pointer to struct with PWLE information
 * @token: Portion of string being analyzed
 * @section: Current section of PWLE being populated
 * @indef: Boolean to set to true if indefinite value (65535) given
 *
 * Converts the time floating point value provided to an integer
 * that can be interpreted by the device driver.
 *
 * Returns 0 upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type12_pwle_time_entry(struct wt_type12_pwle *pwle, char *token,
		struct wt_type12_pwle_section *section, bool *indef)
{
	int ret, val;

	/* Valid values from spec.: 0 ms - 16383.75 ms = infinite */
	ret = parse_float(token, &val, 100, 0, 1638375);
	if (ret) {
		printf("Fauked to parse time: %d\n", ret);
		return ret;
	}

	section->time = val / (100 / 4);

	if (val == WT_TYPE12_PWLE_INDEF_TIME_VAL)
		*indef = true;
	else
		pwle->wlength += section->time;

	return 0;
}

/*
 * wt_type12_pwle_level_entry() - Get level value from string
 *
 * @pwle: Pointer to struct with PWLE information
 * @token: Portion of string being analyzed
 * @section: Current section of PWLE being populated
 *
 * Converts the level floating point value provided to an integer
 * that can be interpreted by the device driver.
 *
 * Returns 0 upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type12_pwle_level_entry(struct wt_type12_pwle *pwle, char *token,
		struct wt_type12_pwle_section *section)
{
	int ret, val;

	/* Valid values from spec.: -1 - 0.9995118 */
	ret = parse_float(token, &val, 10000000, -10000000, 9995118);
	if (ret) {
		printf("Failed to parse level: %d\n", ret);
		return ret;
	}

	section->level = val / (10000000 / 2048);

	return 0;
}

/*
 * wt_type12_pwle_freq_entry() - Get frequency value from string
 *
 * @pwle: Pointer to struct with PWLE information
 * @token: Portion of string being analyzed
 * @section: Current section of PWLE being populated
 *
 * Converts the frequency floating point value provided to an integer
 * that can be interpreted by the device driver.
 *
 * Returns 0 upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type12_pwle_freq_entry(struct wt_type12_pwle *pwle, char *token,
		struct wt_type12_pwle_section *section)
{
	int ret, val;

	/* Valid values from spec.: 0.25 Hz - 1023.75 Hz */
	ret = parse_float(token, &val, 1000, 250, 1023750);
	if (ret) {
		printf("Failed to parse frequency: %d\n", ret);
		return ret;
	}

	section->frequency = val / (1000 / 4);

	return 0;
}

/*
 * wt_type12_pwle_vb_target_entry() - Get value of Back EMF target voltage
 *
 * @pwle: Pointer to struct with PWLE information
 * @token: Portion of string being analyzed
 * @section: Current section of PWLE being populated
 *
 * Converts the Back EMF voltage floating point value provided to an integer
 * that can be interpreted by the device driver.
 *
 * Returns 0 upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type12_pwle_vb_target_entry(struct wt_type12_pwle *pwle,
		char *token, struct wt_type12_pwle_section *section)
{
	int ret, val;

	/*
	 * Don't pass a scale value, since scaling is done locally.
	 * Valid values from spec.: 0 - 1.
	 */
	ret = parse_float(token, &val, 1000000, 0, 1000000);
	if (ret) {
		printf("Failed to parse VB target: %d\n", ret);
		return ret;
	}

	/* Approximation to scaling to 999999/0x7fffff without overflowing */
	val = (val * 1770) / 211;
	val = (val > 0x7FFFFF) ? 0x7FFFFF : val;
	val = (val < 0) ? 0 : val;

	section->vbtarget = val;

	return 0;
}

/*
 * wt_type12_pwle_write() - Populate data buffer with PWLE information
 *
 * @pwle: Pointer to pwle struct being used
 * @buf: Data buffer to be populated with PWLE info
 * @size: Maximum size in bytes of data to be written
 *
 * Write PWLE information to data buffer
 *
 * Returns the number of bytes written to the memory chunk
 *
 */
static int wt_type12_pwle_write(struct wt_type12_pwle *pwle, void *buf,
		int size)
{
	struct dspmem_chunk ch = dspmem_chunk_create(buf, size);
	int i;

	dspmem_chunk_write(&ch, 24, pwle->wlength);
	dspmem_chunk_write(&ch, 8, pwle->repeat);
	dspmem_chunk_write(&ch, 12, pwle->wait);
	dspmem_chunk_write(&ch, 8, pwle->nsections);

	for (i = 0; i < pwle->nsections; i++) {
		dspmem_chunk_write(&ch, 16, pwle->sections[i].time);
		dspmem_chunk_write(&ch, 12, pwle->sections[i].level);
		dspmem_chunk_write(&ch, 12, pwle->sections[i].frequency);
		dspmem_chunk_write(&ch, 8, pwle->sections[i].flags);

		if (pwle->sections[i].flags & WT_TYPE12_PWLE_AMP_REG_BIT)
			dspmem_chunk_write(&ch, 24, pwle->sections[i].vbtarget);
	}

	dspmem_chunk_flush(&ch);

	return dspmem_chunk_bytes(&ch);
}

/*
 * wt_type12_pwle_str_to_bin() - Parse PWLE string to get binary data
 *
 * @full_str: Entire OWT string
 * @data: Data buffer to be populated
 *
 * Converts PWLE string to binary format to be written directly to the
 * device.
 *
 * Returns the number of bytes written to the data buffer upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type12_pwle_str_to_bin(char *full_str, uint8_t *data)
{
	bool t = false, l = false, f = false, c = false, b = false, a = false;
	bool v = false, indef = false;
	unsigned int num_vals = 0, num_segs = 0;
	char delim[] = ",\n";
	int ret = 0, val;
	struct wt_type12_pwle_section *section;
	struct wt_type12_pwle pwle;
	char *str;

	pwle.wlength = 0;
	section = pwle.sections;
	section->flags = 0;

	str = strtok(full_str, delim);
	while (str != NULL) {
		if (num_vals >= WT_TYPE12_PWLE_TOTAL_VALS)
			return -E2BIG;

		switch (wt_type12_pwle_specifier_get(strsep(&str, ":"))) {
		case PWLE_SPEC_SAVE:
			if (num_vals != 0) {
				printf("Malformed PWLE, missing Save\n");
				return -EINVAL;
			}

			ret = wt_type12_pwle_save_entry(str);
			if (ret)
				return ret;
			break;
		case PWLE_SPEC_FEATURE:
			if (num_vals != 1) {
				printf("Malformed PWLE, missing feature\n");
				return -EINVAL;
			}

			ret = wt_type12_pwle_feature_entry(&pwle, str);
			if (ret)
				return ret;
			break;
		case PWLE_SPEC_REPEAT:
			if (num_vals != 2) {
				printf("Malformed PWLE, missing repeat\n");
				return -EINVAL;
			}

			ret = wt_type12_pwle_repeat_entry(&pwle, str);
			if (ret)
				return ret;
			break;
		case PWLE_SPEC_WAIT:
			if (num_vals != 3) {
				printf("Malformed PWLE, incorrect WT slot\n");
				return -EINVAL;
			}

			ret = wt_type12_pwle_wait_time_entry(&pwle, str);
			if (ret)
				return ret;
			break;
		case PWLE_SPEC_TIME:
			if (num_vals > 4) {
				/* Verify complete previous segment */
				if (!t || !l || !f || !c || !b || !a || !v) {
					printf("PWLE Missing entry in seg %d\n",
						(num_segs - 1));
					return -EINVAL;
				}
				t = false;
				l = false;
				f = false;
				c = false;
				b = false;
				a = false;
				v = false;
			}

			ret = wt_type12_pwle_time_entry(&pwle, str, section,
					&indef);
			if (ret)
				return ret;

			t = true;
			break;
		case PWLE_SPEC_LEVEL:
			ret = wt_type12_pwle_level_entry(&pwle, str, section);
			if (ret)
				return ret;

			l = true;
			break;
		case PWLE_SPEC_FREQ:
			ret = wt_type12_pwle_freq_entry(&pwle, str, section);
			if (ret)
				return ret;

			f = true;
			break;
		case PWLE_SPEC_CHIRP:
			val = atoi(str);
			if (val < 0 || val > 1) {
				printf("Valid chirp: 0 or 1\n");
				return -EINVAL;
			}

			if (val)
				section->flags |= WT_TYPE12_PWLE_CHIRP_BIT;

			c = true;
			break;
		case PWLE_SPEC_BRAKE:
			val = atoi(str);
			if (val < 0 || val > 1) {
				printf("Valid Braking: 0 or 1\n");
				return -EINVAL;
			}

			if (val)
				section->flags |= WT_TYPE12_PWLE_BRAKE_BIT;

			b = true;
			break;
		case PWLE_SPEC_AR:
			val = atoi(str);
			if (val < 0 || val > 1) {
				printf("Valid Amplitude Regulation: 0 or 1\n");
				return -EINVAL;
			}

			if (val)
				section->flags |= WT_TYPE12_PWLE_AMP_REG_BIT;

			a = true;
			break;
		case PWLE_SPEC_VBT:
			if (section->flags & WT_TYPE12_PWLE_AMP_REG_BIT) {
				ret = wt_type12_pwle_vb_target_entry(&pwle,
						str, section);
				if (ret)
					return ret;
			}

			v = true;
			num_segs++;
			section++;
			section->flags = 0;
			break;
		default:
			printf("Invald PWLE input, exiting\n");
			return -EINVAL;
		}

		str = strtok(NULL, delim);
		num_vals++;
	}

	/* Verify last segment was complete */
	if (!t || !l || !f || !c || !b || !a || !v) {
		printf("Malformed PWLE: Missing entry in seg %d\n",
				(num_segs - 1));
		return -EINVAL;
	}

	pwle.nsections = num_segs;
	pwle.str_len = strlen(full_str);

	pwle.wlength *= (pwle.repeat + 1);
	pwle.wlength -= pwle.wait;
	pwle.wlength *= 2;

	if (indef)
		pwle.wlength |= WT_INDEFINITE;

	pwle.wlength |= WT_LEN_CALCD;

	return wt_type12_pwle_write(&pwle, data, WT_TYPE12_PWLE_BYTES_MAX);
}

/*
 * get_owt_data() - Determines the waveform type of the string to decode
 *
 * @full_str: Entire OWT string
 * @data: Data buffer to be populated
 *
 * Determines if the OWT string provided is of Type 10 (Composite) or Type 12
 * (PWLE) format and calls the corresponding function to convert the string
 * appropriately.
 *
 * Returns the number of bytes written to the data buffer upon success.
 * Returns negative errno in error case.
 *
 */
int get_owt_data(char *full_str, uint8_t *data)
{
	int num_bytes;

	printf("Full String = %s\n", full_str);
	/* check if PWLE or Composite via first character */
	if (full_str[0] == 'S')
		num_bytes = wt_type12_pwle_str_to_bin(full_str, data);
	else
		num_bytes =  wt_type10_comp_str_to_bin(full_str, data);

	return num_bytes;
}
