# NeoDK
An electrostimulation machine development kit.

### What is it?
This project describes the design and implementation of an innovative and powerful pulse-based electrostimulation device. Both its hardware and its firmware are open for modification and extension.

### Author
Discord: @Onwrikbaar<br/>
E: onwrikbaar@hotmail.com

### Goals
1. Innovation in the e-stim realm has been virtually nonexistent for a quarter century, despite massive advances in electronics. Currently available commercial devices are without exception channel-oriented and produce TENS-like waveforms. It is time to get rid of those limitations.
2. It is fairly easy for non-programmers to create, share and improve routines to be played on stereostim setups. Currently there is no way to do the same for pulse-based e-stim devices. This project aims to (eventually) bridge that gap.

### Disclaimer
NeoDK is experimental, recreational hardware and software. It comes with absolutely no warranty of any kind.
- Use it entirely at your own risk.
- Do not use it for anything time-critical, safety-critical, medical, industrial, military, commercial, or illegal - it was not designed for any of that.
- Do not use it on anyone without explicit and informed consent.
- Keep any and all electrodes connected to this device below the waist.

### Safety
The NeoDK device can generate potentially lethal voltages and currents on its outputs and on internal components, even when powered from a battery. The risks associated with building, testing and using this device can be reduced but not eliminated.

The NeoDK device is designed to stay well within the IEC 60601-2-10 requirements. However, these limits are not necessarily maintained when the circuit, components or accompanying firmware are changed in any way.

### Audience
This project is intended for experienced electronics hobbyists who possess a keen understanding of the risks associated with electrostimulation and high-voltage circuitry, who
- are looking for e-stim sensations unattainable through existing power boxes.
- want a pulse-based device that can store and play user-created routines.
- would like to be able to create custom routines without too much hassle.

### Design requirements
1. Outputs galvanically isolated from the low-voltage circuitry.
2. Four monopolar outputs, no 'channels'.
3. Sufficient output power to drive even the largest internal electrodes with high intensity.
4. Battery-powered.
5. Controllable by externally running software.

### Electrical characteristics (preliminary)
- NeoDK outputs rectangular pulses with a maximum voltage of 100V across a 500 Ω load. Pulses can have either of the two polarities, so Vpp is 200V.
- Maximum pulse width is 200 µs.
- Maximum charge per pulse is 40 µC for a 500 Ω load.
- Maximum energy into a 500 Ω load is 4 mJ per pulse.
- Maximum pulse repetition rate is 200 Hz.
- Maximum output power into a 500 Ω load is 800 mW. This corresponds to a current of 40 mA RMS.

### Output configurations
The four unipolar outputs A, B, C and D can be switched under software control in the following nine ways (X-Y means current can flow from electrode X to electrode Y, or from electrode Y to electrode X):
- A-B
- C-B
- AC-B
- A-D
- C-D
- AC-D
- A-BD
- C-BD
- AC-BD

Every single pulse can be directed to a different electrode configuration, if so desired. This allows the creation of stimulation patterns that are perceived as moving through the body.

### The output transformer
- A supply voltage of around 10V must be stepped up by a factor of 10 to reach the desired output voltage of 100V. Since we only drive half of the primary coil at a time, the ratio of the whole primary to the secondary needs to be 1:5.
- The transformer must have a power rating of around 800 mW (see electrical characteristics above).
- The maximum secondary current through a 500 Ω load is 100V/500 Ω = 0.2 A. The primary current is then 10 * 0.2 A = 2A. To limit the ohmic losses at this current, the DC-resistance of the primary should be less than 1 Ω.
- The transformer needs to have a bandwidth of at least a few kHz in order to handle the rectangular pulses well.

An off-the-shelf, cheap transformer that meets these criteria, is the Xicon 42TU200, which is carried by Mouser Electronics.

### The buffer capacitor
When the device is powered from a battery that cannot immediately deliver the required primary current, a low-ESR buffer capacitor is needed to supply the output stage. We use two 220 µF 16V tantalum capacitors in parallel.
