# The Pulse Train Descriptor
## Purpose
Pulse train descriptors are a means to efficiently stream in real time electrostimulation patterns from a phone or computer to a power box. A pulse train descriptor specifies the output stage, polarity, electrode configuration, timing, and intensity of a sequence of pulses.
## Structure
A pulse train descriptor consists of up to 11 members, occupying up to 16 bytes. Multi-byte members are stored little-Endian.
```
    uint8_t  meta;                  // Type, version, flags, etc., for correct interpretation of this descriptor.
    uint8_t  sequence_number;       // For diagnostics. Wraps around to 0 after 255.
    uint8_t  phase;                 // Bits 2..1 select 1 of 4 biphasic output stages. Bit 0 is the selected stage's polarity.
    uint8_t  pulse_width_µs;        // The duration of one pulse [µs].
    uint32_t start_time_µs;         // When this burst should begin, relative to the start of the stream.
    uint8_t  electrode_set[2];      // The (max 8) electrodes connected to each of the two output phases of the selected output stage.
    uint16_t nr_of_pulses;          // Length of this burst.
    uint8_t  pace_¼ms;              // [0.25 ms] time between the start of consecutive pulses.
    uint8_t  amplitude;             // Voltage, current or power, in units of 1/255 of the set maximum.
    // The following members [-128..127] are applied after each pulse of this train.
    int8_t   delta_pulse_width_¼µs; // [0.25 µs]. Changes the duration of the pulses.
    int8_t   delta_pace_µs;         // [µs]. Modifies the time between pulses.
```
## Descriptor size and interpretation
- If `delta_pace_µs` = 0, it may be omitted.
- If `delta_pace_µs` = 0 and `delta_pulse_width_¼µs` = 0, they may both be omitted.
- `amplitude` = 0 means 'the same amplitude as the previous descriptor', or 'amplitude is set through other means'. It does _not_ set the output level to 0.
- If `delta_pace_µs` = 0 and `delta_pulse_width_¼µs` = 0 and `amplitude` = 0, all three may be omitted.
- If `nr_of_pulses` = 1, `pace_¼ms` is ignored and may be omitted if `amplitude`, `delta_pulse_width_¼µs` and `delta_pace_µs` are omitted. In this case `nr_of_pulses` may be omitted as well and will be taken to be 1.
- For hardwired output stages (i.e. without any sort of switch matrix between the transformers and the electrodes) `electrode_set` is not used. It may be omitted if all the members following it are omitted.
- Summarising the above, the minimal pulse train descriptor contains only the first 5 members and is 8 bytes in size.
- `meta` must be set to 0x00, for now.
- Bits 7..3 of `phase` are reserved for future use.
## Example: generating TENS-style pulses
1. Send a pulse train, followed by the same pulse train time-shifted and with opposite phase.<br/>
Pt 1: t=  0 µs, ec=0x5<>0xa, phase=0, np=50, pace=80 ¼ms, amp=0, pw=130 µs, Δ=0 ¼µs<br/>
Pt 2: t=180 µs, ec=0x5<>0xa, phase=1, np=50, pace=80 ¼ms, amp=0, pw=130 µs, Δ=0 ¼µs<br/>
The time shift needs to be at least the pulse width plus the minimum dead time.
2. Send the command to execute the received pulse trains. This implies that the firmware be able to buffer several pulse train descriptors.
