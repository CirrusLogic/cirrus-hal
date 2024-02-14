The Open Wavetable library is used by Android HAL code to facilitate interaction with CS40L26.
It standardizes a way of representing Composite or PWLE waveforms, and allows the user
to easily write those waveforms to the device's RAM wavetable at runtime.

# Type 10 (Composite) waveforms

Composite waveforms are made up of waveforms that are loaded in either the static RAM wavetable at
boot time (from cs40l26.bin) or the ROM wavetable (pre-existing on the device). Note that
composites cannot contain other composite waveforms. There are checks in place in the driver to
ensure that any invalid input is rejected.

| Symbol | Type | Description |
| ------------- | ------------- | ------------- |
| !! | Inner loop start  | Two back-to-back exclamation points indicate the beginning of an inner loop. When the queue advances to a subsequent inner loop stop indicator, it returns to the next waveform or pause indicator immediately following the previous inner loop start indicator. Each inner loop start indicator must be followed by a complementary inner loop stop indicator. |
| n | Pause  | A lone number is interpreted as a pause indicator, with n representing the number of milliseconds (0 through 10,000) to wait in silence. |
| n! | Outer loop repeat (finite) | A number followed by a single exclamation point indicates that the entire queue (outer loop) is to be repeated, with n representing the number of times to repeat the outer loop (0 through 32). A finite outer loop repeat indicator can be placed anywhere in the queue, but only one may be present. It is forbidden to include a finite outer loop repeat indicator if an infinite outer loop repeat indicator is already present. |
| n!! | Inner loop stop | A number followed by two back-to-back exclamation points indicates the end of an inner loop, with n representing the number of times to repeat the inner loop (0 through 32). When the queue encounters an inner loop stop indicator, it returns to the next waveform or pause indicator immediately following the previous inner loop start indicator. Each inner loop stop indicator must be preceded by a corresponding inner loop start indicator. |
| Xm.n | Waveform | Two numbers separated by a period are interpreted as a waveform indicator, with m representing the corresponding value of the index in the RAM wavetable to be rendered and n representing the percent scaling to be applied (1 through 100). Neither m nor n may be equal to zero; this is for backward compatibility with some legacy Cirrus Logic parts. An optional string X may be prepended to the index to specify either a ROM or RAM bank. . If no string is provided, the default is the RAM bank. |
| ~ | Outer loop repeat (infinite) | A single tilde indicates that the entire queue (outer loop) is to be repeated indefinitely until canceled. An infinite outer loop repeat indicator can be placed anywhere in the queue, but only one may be present. It is forbidden to include an infinite outer loop repeat indicator if a finite outer loop repeat indicator is already present. |

# Type 12 (PWLE) waveforms

PWLE waveforms create a piecewise linear envelope wave that can have varying voltage levels,
frequencies, and Back EMF voltage targets across its individual segments. These can be written to
the RAM wavetable and triggered without using previously loaded waveforms as in the composite case.

| Symbol | Values | Description |
| ------------- | ------------- | ------------- |
| S | 0 or 1 | Unused but must be present. |
| WF | 0 - 255 | Indicate what features are present in the waveform. The value for this symbol is an integer representation of the WF bit field described below this table. |
| RP | 0 - 255 | The number of times the entire waveform will play if not interrupted. |
| WT | 0 - 1023.75 | Amount of time in ms to wait before repeating playback. 0.25 ms resolution. |
| M | -1, 0, 1, 2, 3 | SVC braking mode. -1: None, 0: CAT, 1: Closed loop, 2: Open loop, 3: Mixed mode |
| K | 0 - 1000 | SVC braking time in ms. |
| EM | 0 or 1 | EP threshold mode. 0: Fixed or default threshold, 1: Custom threshold |
| ET | 0 - 7 | EP threshold. The value for this symbol is an integer representation of the EP bit field described below this table. |
| EC | >= 0| EP Custom threshold (if EM = 1 and bits 2:1 of ET = 0). |
| T# | 0 - 16383.5 or 16383.75 | The time at which the section corresponding to # will start in ms. An indefinite value will continue playing the section until interrupted. 0.25 ms resolution. |
| L# | -1 - 0.9995 | Intensity level, negative values cause a 180-degree phase shift. 0.00048 resolution. |
| F# | 0 - 1023.75 | Sets the synthesized frequency of the PWLE section. 0.25 Hz resolution. For other sections than the first one, if this item is set to 0x0, the frequency is not fixed and instead is set to the resonant frequency of the actuator (F0). |
| C# | 0 or 1 | If 0, the frequency defined with F# is constant. If 1, the frequency ramps linearly up or down starting with frequency given with F(#-1), level L(#-1), at time T(#-1), and ends with frequency F#, level L# at time T#. |
| B# | 0 or 1 | Unused but must be present. |
| AR# | 0 or 1 | If 0, amplitude regulation is disabled for this section. (V#) must be 0. If 1, amplitude regulation is enabled for this section and VbTarget (V#) must be set. |
| R# | 0 or 1 | If 1, the frequency value "F#" is interpreted as a signed offset applied to the resonant frequency (F0). The resulting output frequency is F0 + F# + VIBEGEN_F0_OFFSET which is a global value present in the firmware. |
| V# | 0 or 1 | Target back EMF value for amplitude regulation. This can only be non-zero if amplitude regulation (AR#) is set to 1. |

Waveform feature (WF) bitfield:
| Bit No | Purpose | 0 | 1 |
| ----- | ----- | ----- | ----- |
| 7 | Waveform type | Buzz | Click |
| 6 | Unused | N/A | N/A |
| 5 | Unused | N/A | N/A |
| 4 | Dynamic F0 | Disabled | Enabled |
| 3 | Unused | N/A | N/A |
| 2 | Metadata for SVC or EP | No metadata | Metadata present |
| 1 | DVL | Disabled | Enabled |
| 0 | LF0T | Disabled | Enabled |

EP Threshold bitfield:
| Bit No  | 00 | 01 | 10 | 11 |
| ------------- | ------------- | ----- | ------ | ---- |
| 0      | No excursion limit applied     | Excursion limit applied | N/A | N/A |
| 2:1      | Default/custom threshold     | Fixed threshold #1 | Fixed threshold #2 | Fixed Threshold #3 |

See main.c for examples of how to use this library with each type of OWT waveform.