# NeoDK
An electrostimulation machine development kit.

### What is it?
This project describes the design and implementation of a bare-bones, yet innovative and powerful, pulse-based electrostimulation device. Both its hardware and its firmware are open for modification and extension.

### Author
Discord: @Onwrikbaar<br/>
E: onwrikbaar@hotmail.com

### Goals
1. The last quarter century, innovation in the e-stim realm has been virtually nonexistent, despite massive advances in electronics and cheap computing power. Currently available commercial devices are without exception channel-oriented and produce TENS-like waveforms. It is time to get rid of those historical remnants.
2. It is fairly easy for non-programmers to create, share and improve routines to be played on stereostim setups. Currently there is no way to do the same for pulse-based e-stim devices. This project aims to (eventually) bridge that gap.

### Disclaimer
NeoDK is experimental, recreational hardware and software. It comes with absolutely no warranty of any kind.
- Use it entirely at your own risk.
- Do not use it for anything time-critical, safety-critical, medical, industrial, military, commercial, or illegal - it was not designed for any of that.
- Do not use it on anyone without explicit and informed consent.
- Keep any and all electrodes connected to this device placed below the waist.

### Safety
The NeoDK device can generate potentially lethal voltages and currents on its outputs and on internal components, even when powered from a battery. The risks associated with building, testing and using this device can be reduced but not eliminated.

The NeoDK device is designed to stay well within the IEC 60601-2-10 requirements. However, these limits are not necessarily maintained when the circuit, components or accompanying firmware are changed in any way.

### Audience
This project is intended for experienced electronics hobbyists who possess a keen understanding of the risks associated with electrostimulation and high-voltage circuitry, who desire one or more of the following:
- e-stim sensations unattainable through existing power boxes.
- create one's own pulse-based e-stim box, based on a rock solid and well-tested design.
- a pulse-based device that can store and play user-created routines.
- be able to create custom routines without too much hassle.

### Caveat
NeoDK is a minimal viable design (MVD) of a powerful and highly efficient electrostimulation device that is not limited by 2-pole 'channels' or strictly TENS-like waveforms. It can thus do things no commercially available e-stim box comes close to. If you still would like to have more features than NeoDK has, by all means use NeoDK as a foundation for your own design.

### Nothing but the truth
"A complex system that works is invariably found to have evolved from a simple system that worked. A complex system designed from scratch never works and cannot be patched up to make it work. You have to start over with a working simple system." -- John Gall

### Design requirements
1. Outputs galvanically isolated from the low-voltage circuitry.
2. Four monopolar biphasic outputs, no 'channels'.
3. Sufficient output power to drive even the largest internal electrodes with high intensity.
4. Battery-powered, and very energy-efficient.
5. Controllable by externally/remotely running software.

### Electrical characteristics (preliminary)
- NeoDK outputs rectangular pulses with a maximum voltage of 100V across a 500 Ω load. Pulses can have either of the two polarities, so Vpp is 200V.
- Maximum pulse width is 200 µs.
- Maximum charge per pulse is 40 µC for a 500 Ω load.
- Maximum energy into a 500 Ω load is 4 mJ per pulse.
- Maximum pulse repetition rate is 200 Hz.
- Maximum output power into a 500 Ω load is 800 mW. This corresponds to a current of 40 mA RMS.

### Output configurations
The four unipolar outputs A, B, C and D can be switched under software control in the following nine ways (X-Y means current can flow from electrode X to electrode Y, or vice versa. X-YZ means current flows from electrode X to electrodes Y and Z simultaneously):
- A-B
- C-B
- AC-B
- A-D
- C-D
- AC-D
- A-BD
- C-BD
- AC-BD

Every single pulse can be directed to a different electrode configuration of 2, 3 or 4 electrodes, if so desired. This allows the creation of stimulation patterns that are perceived as moving through the body.

## Electronic design
### The output transformer
- A supply voltage of about 10V must be stepped up by a factor of 10 to reach the desired output voltage of 100V. Since we only drive half of the (center-tapped) primary coil at a time, the ratio of the whole primary to the secondary needs to be 1:5.
- The transformer must have a power rating of around 800 mW (see electrical characteristics above).
- The maximum secondary current through a 500 Ω load is 100V/500 Ω = 0.2 A. The primary current is then 10 * 0.2 A = 2A. To limit the ohmic losses at this current, the DC-resistance of the primary should be less than 0.5 Ω.
- The transformer needs to have a bandwidth of at least a few kHz in order to handle the rectangular pulses well enough.

An off-the-shelf, cheap transformer that meets these criteria, is the Xicon 42TU200, which is carried by Mouser Electronics.

### Switch matrix
The output voltage of the transformer is distributed to the four electrodes through a 4-node switch matrix consisting of opto-triacs (aka photo-triacs or triac output optocouplers).

### Buffer capacitor
When the device is powered from a battery that cannot immediately deliver the required primary current, a low-ESR buffer capacitor is needed to help supply the output stage. We use two or three 220 µF 16V tantalum capacitors in parallel.

### Snubber
Stray inductance of the transformer causes inductive spikes every time the primary current is switched off. These spikes must be suppressed in order to prevent destruction of the MOSFETs switching the transformer's primary. This is accomplished by 'shorting' the spikes to the Vcap rail when they exceed a critical value.

### Primary current sensing
The primary current is measured on the high side, by means of a 30 mΩ shunt resistor and a x20 current sense amplifier. The output of the amplifier is fed to an ADC input of the microcontroller.

### Primary voltage sensing
The primary voltage is measured through a ÷4 voltage divider connected to an ADC input of the microcontroller.

### Primary voltage control
The primary voltage is regulated by a (switching) buck converter, which is controlled through a DAC output of the microcontroller. This method ensures very efficient use of the battery capacity as well as negligible heat generation.

### Microcontroller
The STM32G071 is an affordable 32-bit controller with an ARM Cortex M0+ core, 128 KB flash and 36 KB RAM.

## Firmware

### Power-on selftest
### Built-in routines
