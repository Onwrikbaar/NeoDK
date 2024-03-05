# Design
## Functional design
### Requirements
1. Outputs galvanically isolated from the low-voltage circuitry and the communication interface.
2. Four monopolar symmetric biphasic outputs, no 'channels'.
3. Sufficient output power to drive even the largest internal metal electrodes with amazing intensity.
4. Very energy-efficient, so optionally battery-powered.
5. Controllable by externally/remotely running software.

### Electrical characteristics (preliminary)
- NeoDK outputs rectangular pulses with a maximum voltage of 100V across a 500 Ω load. Pulses can have either of the two polarities, so Vpp is 200V.
- Maximum pulse width is 200 µs.
- Maximum charge per pulse is 40 µC for a 500 Ω load.
- Maximum energy into a 500 Ω load is 4 mJ per pulse.
- Maximum pulse repetition rate is 200 Hz.
- Maximum dutycycle is 200 µs * 200 Hz * 100% = 4%.
- Maximum output power into a 500 Ω load is 800 mW. This corresponds to a current of 40 mA RMS.
- Average current draw from the power supply or battery is less than 200 mA.

### Output configurations
The four biphasic unipolar outputs A, B, C and D can be switched under software control in the following nine ways (X-Y means current can flow from electrode X to electrode Y, or vice versa. X-YZ means current flows from electrode X to electrodes Y and Z simultaneously):
- A-B
- C-B
- AC-B
- A-D
- C-D
- AC-D
- A-BD
- C-BD
- AC-BD

Every single pulse can be directed to a different electrode configuration of 2, 3 or 4 electrodes, if so desired. This allows the creation of stimulation patterns that are perceived as moving across the skin or in 3D through the body.

## Electronic design
### The output transformer
- A supply voltage of about 10V must be stepped up by a factor of 10 to reach the desired output voltage of 100V. Since we only drive half of the (center-tapped) primary coil at a time, the ratio of the whole primary to the secondary needs to be 1:5.
- The transformer must have a power rating of around 800 mW (see electrical characteristics above).
- The maximum secondary current through a 500 Ω load is 100V/500 Ω = 0.2 A. The primary current is then 10 * 0.2 A = 2A. To limit the ohmic losses at this current, the DC-resistance of the primary should be at most 0.5 Ω.
- The transformer needs to have a bandwidth of at least a few kHz in order to handle the rectangular pulses well enough.

An off-the-shelf, cheap transformer that meets these criteria, is the Xicon 42TU200, which is carried by [Mouser Electronics](https://www.mouser.com).

### Switch matrix
The output voltage of the transformer is distributed to the four electrodes through a 4-node switch matrix consisting of opto-triacs (aka photo-triacs or triac output optocouplers).

### Buffer capacitor
When the device is powered from a battery that cannot immediately deliver the required primary current, a low-ESR buffer capacitor is needed to help supply the output stage. We use three 220 µF 16V tantalum capacitors in parallel.

### Snubber
Stray inductance of the transformer causes inductive spikes every time the primary current is switched off. These spikes must be suppressed in order to prevent destruction of the MOSFETs switching the transformer's primary. This is accomplished by 'shorting' the spikes to the primary voltage rail when they exceed a critical value. This way part of the energy stored in the transformer's inductance is dumped back into the capacitors.

### Primary current sensing
The primary current is measured on the high side, by means of a 20 mΩ shunt resistor and a x20 current sense amplifier. The output of the current sense amplifier is fed to an ADC input of the microcontroller. This way, the maximum primary current that can be reliably measured is 3.3V / (20 * 20 mΩ) = 8.25 A.

### Primary voltage sensing
The primary voltage is measured through a ÷4 voltage divider connected to an ADC input of the microcontroller. This way, the maximum primary voltage that can be reliably measured is 4 * 3.3V = 13.2V.

### Primary voltage control
The primary voltage is regulated by a (switching) buck converter, which is controlled through a DAC output of the microcontroller. This method ensures very efficient use of the battery capacity as well as negligible heat generation. The buck's maximum under-voltage lockout threshold is 4.95V, so the device requires a supply voltage of at least 5V for correct operation.

### Microcontroller
The STM32G071 is an affordable 32-bit microcontroller with an ARM Cortex M0+ core, 128 KB flash and 36 KB RAM. Its maximum clock speed is 64 MHz, which yields considerably more processing power than required to run the application.
- [datasheet](https://www.st.com/resource/en/datasheet/stm32g071c8.pdf)
- [reference manual](https://www.st.com/resource/en/reference_manual/rm0444-stm32g0x1-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)

### Recommended power source
For safety, it is best to power NeoDK from a battery, like a 3S (nominally 11.1V) Li-ion battery pack. When using a mains voltage adapter ('wall wart') instead, make sure it is doubly insulated and meets all applicable safety standards for your region. If you happen to own an Estim Systems 2B power box, you can use its 12V mains adapter for NeoDK too.

### Serial communications interface
The device is controlled through a standard serial UART interface using 3.3V TTL signal levels at 57600 bps. Suitable USB-to-serial cables (3.3V TTL, Tip=Tx, Ring=Rx, Sleeve=GND) are readily available from various sources, like Aliexpress, for about €7 including shipping.

## Firmware
The device's on-board control program (aka firmware) consists of a collection of collaborating state machines. The firmware is event-driven and 100% nonblocking.

### Structure
Conceptually, the firmware consists of two layers:
1. The hardware-dependent part, called the Board Support Package (BSP), which implements the _mechanisms_. This is the only module that needs to be changed when porting the application to a different processor.
2. The hardware-independent application logic, implementing the _policies_.
