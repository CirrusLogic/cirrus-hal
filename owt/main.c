/*
 * Copyright 2023 Cirrus Logic Inc.
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

#include <fcntl.h>
#include "owt.h"

int main(int argc, char **argv)
{
	uint8_t data[WT_TYPE12_PWLE_SINGLE_PACKED_MAX];
	char str[WT_STR_MAX_LEN];
	int num_bytes, i, fd, ret;
	struct ff_effect effect;

	/*
	 * This composite string will result in index 1 from the RAM wavetable being played at 100%
	 * intensity followed by a 500 ms delay, then index 2 from the ROM wavetable would be
	 * played at 100% intensity followed by a 400 ms delay. Waveforms at index 3 from ROM at
	 * 50%, 75%, and 100% are played with a 50 ms delay between them. The 1!! marker denotes
	 * that this section is repeated once for a total of 2 playthroughs. Finally, 2! marks that
	 * the entire string will play 2 more times for a total of 3 playthroughs. The driver
	 * calculates the duration based on the binary information provided by the Userspace
	 * program and plays the composite waveform in its entirety unless interrupted by the user.
	 */
	memset(str, '\0', sizeof(str));
	strcpy(str, "1.100, 500, ROM2.100, 400, !!, ROM3.50, 50, ROM3.75, 50, ROM3.100, 50, 1!!, 2!");

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
		return -1;
	}

	ret = owt_upload(data, num_bytes, 0, fd, false, &effect);
	if (ret < 0) {
		printf("Failed to upload OWT effect\n");
		return ret;
	}

	printf("Triggering Composite Waveform\n");
	/* Trigger effect */
	ret = owt_trigger(effect.id, fd, true);
	if (ret)
		return ret;

	printf("Press any key to exit OWT trigger\n");
	getchar();

	/* Stop Playback */
	ret = owt_trigger(effect.id, fd, false);
	if (ret)
		return ret;

	/*
	 * The example simple PWLE waveform is designed as follows.
	 * S:0 is a placeholder for CS40L26, this value could also be S:1 and would have no effect
	 * as all OWT waveforms are written to RAM. WF:0 marks the effect as a buzz; this is the
	 * information used by other waveforms, and it does not affect the behavior of the designed
	 * PWLE. The entirety of the PWLE will be played twice since one repeat has been assigned
	 * via RP:1. The wait time between repeats will be 399.5 ms (WT:399.5). M:-1 and K:0
	 * indicates that SVC braking will not be in use, and the braking time will be ignored.
	 *
	 * The next symbols are section specific. T0:0 indicates that section 0 starts at 0 ms;
	 * L0:0.49152 gives the intensity level of section 0. This section will play at 200 Hz and
	 * is not designated as a chirp, meaning the frequency will be constant; these parameters
	 * are set with F0:200 and C0:0. Rounding out section 0’s parameters, B0:0 indicates that
	 * there is no braking but, as mentioned in the above table, this value has no effect for
	 * CS40L26. R0:0 indicates that no offset be applied to the resonant frequency. Furthermore,
	 * section 0 will not be using amplitude regulation: A0:0, V0:0.
	 *
	 * Section 1 is the final section of the PWLE waveform. The string enables section 1 at
	 * 400 ms (400 ms after section 0 was started) T1:400. The frequency and intensity level
	 * remains the same as the previous section with L1:0.49152, F1:200 and once again there
	 * are no braking or chirp capabilities (C1:0, B1:0); recall that braking has no effect on
	 * CS40L26. Section 1 does have amplitude regulation enabled (AR1:1) and sets a back EMF
	 * voltage target of 0.022 V (V1:0.022).
	 */

	memset(str, '\0', sizeof(str));
	strcpy(str, "S:0,WF:0,RP:1,WT:399.5,M:-1,K:0,T0:0,L0:0.49152,F0:200,C0:0,B0:0,AR0:0,R0:0,\
	       V0:0,T1:400,L1:0.49152,F1:200,C1:0,B1:0,AR1:1,R1:0,V1:0.022");

	memset(data, 0, WT_TYPE12_PWLE_SINGLE_PACKED_MAX);
	num_bytes = get_owt_data(str, data);

	for (i = 0; i < num_bytes; i++)
		printf("Data[%d] = 0x%02X\n", i, data[i]);

	ret = owt_upload(data, num_bytes, 0, fd, false, &effect);
	if (ret < 0) {
		printf("Failed to upload OWT effect\n");
		return ret;
	}

	printf("Triggering PWLE Waveform\n");
	/* Trigger effect */
	ret = owt_trigger(effect.id, fd, true);
	if (ret)
		return ret;

	printf("Press any key to exit OWT trigger\n");
	getchar();

	/* Stop Playback */
	ret = owt_trigger(effect.id, fd, false);
	if (ret)
		return ret;

	return 0;
}