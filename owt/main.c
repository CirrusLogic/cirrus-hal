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

#include "owt.h"

int main(int argc, char** argv)
{
	uint8_t data[WT_TYPE12_PWLE_SINGLE_PACKED_MAX];
	char str[WT_STR_MAX_LEN];
	int num_bytes, i, fd, effect_id, ret;

	/*
	 * See README for detailed information on Open Wavetable string
	 * formatting.
	 */

	/* Composite waveform which will play RAM index 3 @ 75% amplitude,
	 * wait 100 ms, play index 3 @50% amplitude, wait 100 ms, play index 3
	 * @ 25% amplitude. This will be repeated once for a total of 2
	 * playthroughs.
	 */
	memset(str, '\0', sizeof(str));
	strcpy(str, "3.75, 100, 3.50, 100, 3.25, 100, 1!");

	num_bytes = get_owt_data(str, data);
	if (num_bytes <= 0) {
		printf("Failed to get data for Open Wavetable\n");
		return num_bytes;
	}

	for (i = 0; i < num_bytes; i++)
		printf("Data[%d] = 0x%02X\n", i, data[i]);

	/* Open Input Force Feedback Device, path given is an example */
	fd = open("/dev/input/event1", O_RDWR);
	if (fd == -1) {
		printf("Failed to open Input FF device\n");
		return -ENOENT;
	}

	effect_id = owt_upload(data, num_bytes, fd);
	if (effect_id < 0) {
		printf("Failed to upload OWT effect\n");
		return effect_id;
	}

	printf("Triggering Composite Waveform\n");
	/* Trigger effect */
	ret = owt_trigger(effect_id, fd, true);
	if (ret)
		return ret;

	printf("Press any key to exit OWT trigger\n");
	getchar();

	/* Stop Playback */
	ret = owt_trigger(effect_id, fd, false);
	if (ret)
		return ret;

	/*
	 * The example simple PWLE waveform is designed as follows.
	 * S:0 is not used but is required for backwards compatibility of
	 * this feature and distinguishing between PWLE and Composite waveform
	 * types.
	 * WF:0 Marks the effect as a buzz; it does not affect the behavior
	 * of the designed PWLE.
	 * The entirety of the waveform will be played twice since 1 repeat
	 * has been assigned via RP:1.
	 * The wait time between repeats will be 399.5 ms (WT:399.5).
	 *
	 * The next symbols are section specific. T0:0 indicates that section
	 * 0 starts 0 ms after the playback begins (immediately); L0:0.49152
	 * gives the intensity level of section 0. This section will play at
	 * 200 Hz and is not designated as a chirp, meaning the frequency will
	 * be constant; these parameters are set with F0:200 and C0:0.
	 * B0:0 indicates there is no braking this is similar to the 'S'
	 * parameter in that it has no effect but must be a valid value for
	 * completeness.
	 * Section 0 does not define amplitude regulation: A0:0, V0:0.
	 *
	 * Section 1 is the final section of this PWLE waveform. The parameters
	 * describe the end target values for those provided in section 0.
	 *
	 * The string enables section 1 at 400 ms, which means, since it is the
	 * final section, that one play through of this waveform is 400 ms.
	 * T1:400.
	 * The frequency and intensity level are the same as section 0:
	 * L1:0.49152, F1:200 and once again there is no braking or chirp
	 * capabilities (C1:0, B1:0).
	 * Section 1 does have amplitude regulation enabled (AR1:1)
	 * and sets a back EMF voltage target of 0.022 V (V1:0.022).
	 *
	 * The resulting playback of triggering this waveform will be a 400 ms
	 * buzz with a constant level and frequency. Amplitude regulation is
	 * enabled and targets a Back EMF voltage of 0.022 V. The waveform will
	 * repeat once.
	 */

	memset(str, '\0', sizeof(str));
	strcpy(str, "S:0,WF:8,RP:1,WT:399.5,T0:0,L0:0.49152,F0:200,C0:0,B0:0,AR0:0,V0:0,T1:400,L1:0.49152,F1:200,C1:0,B1:0,AR1:1,V1:0.022");

	memset(data, 0, WT_TYPE12_PWLE_SINGLE_PACKED_MAX);
	num_bytes = get_owt_data(str, data);

	for (i = 0; i < num_bytes; i++)
		printf("Data[%d] = 0x%02X\n", i, data[i]);

	effect_id = owt_upload(data, num_bytes, fd);
	if (effect_id < 0) {
		printf("Failed to upload OWT effect\n");
		return effect_id;
	}

	printf("Triggering PWLE Waveform\n");
	/* Trigger effect */
	ret = owt_trigger(effect_id, fd, true);
	if (ret)
		return ret;

	printf("Press any key to exit OWT trigger\n");
	getchar();

	/* Stop Playback */
	ret = owt_trigger(effect_id, fd, false);
	if (ret)
		return ret;

	return 0;
}