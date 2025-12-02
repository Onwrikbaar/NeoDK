## Updating the firmware without using a debugging probe
1. Download and install STM's free [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html) programming tool.
2. Get [neodk_g071.hex](https://github.com/Onwrikbaar/NeoDK/tree/main/firmware/build/neodk_g071.hex). Click the "Download raw file" icon to download.
3. Connect NeoDK to a computer using a USB-to-3.5mm-TRS-serial cable. Suitable cables (3.3V TTL, Tip=Tx, Ring=Rx, Sleeve=GND) are readily available from various sources, like Aliexpress, for about €10 including shipping.
4. Press and hold the pushbutton on NeoDK while switching on its power (or connecting the battery). This puts NeoDK in bootloader mode. Release the button.
5. Open a terminal window and execute the following on the command line:<br/>
&nbsp;&nbsp;`STM32_Programmer_CLI -c port=/dev/tty.usbserial-0001 -w neodk_g071.hex -v`<br/>
Specify the COM port that corresponds to your USB-serial cable (for instance `COM7` on Windows), in place of `/dev/tty.usbserial-0001`.<br/>
A power cycle starts the firmware; the blue LED should light up and the box is ready for use.

## Controlling NeoDK from a computer
1. Connect NeoDK to a computer using a USB-to-3.5mm-TRS-serial cable (3.3V TTL, Tip=Tx, Ring=Rx, Sleeve=GND).
2. Open a Chrome, Edge or Opera browser window and point it to [@edger477's colorful UI](https://edger477.github.io/NeoDK/UI).

## Developing code for NeoDK
### Debugging probe
A suitable probe is the Segger JLink or Segger JLink EDU. Other probes supporting the SWD interface will work too.

### Compiler
1. Download and install the [Arm GNU Toolchain](https://developer.arm.com/Tools%20and%20Software/GNU%20Toolchain) for bare metal targets.
2. Edit [Makefile.posix](toolchain/gcc/Makefile.posix) or [Makefile.windows](toolchain/gcc/Makefile.windows) (depending on your operating system) to set three environment variables to their appropriate values. Alternatively, they can be set in the startup file for your command line interface.

### STM's low-level code library
1. Download and install [STM32Cube for STM32G0 series](https://www.st.com/en/embedded-software/stm32cubeg0.html).
2. In your home directory, create a softlink to directory 'STM32Cube' of the just installed tree.

### SEGGER J-Link software
1. Download and install [Segger J-Link](https://www.segger.com/downloads/jlink/).
2. In the just installed Segger directory tree, find file JLink_V792k/Samples/RTT/SEGGER_RTT_V792k.tgz and unpack it in place.
3. In your home directory, create a softlink named 'SEGGER_RTT' to directory 'JLink_V792k/Samples/RTT/SEGGER_RTT_V792k/RTT' of the just installed tree.

### Checking your installation
&nbsp;&nbsp;`cd NeoDK/firmware/neodk`<br/>
In this directory ('firmware'), at the command prompt type

&nbsp;&nbsp;`make`

This should compile the demo code without errors and create file 'neodk_g071.hex' (and a few others) in directory 'build'.

### Updating the firmware using Segger JLink
Connect the JLink to a USB port of your computer, and to the 10-pin SWD header on the NeoDK board.
On the commandline type<br/>
&nbsp;&nbsp;`cd firmware/neodk`<br/>
&nbsp;&nbsp;`JLinkExe -device stm32g071kb -if SWD -speed 4000 -autoconnect 1`<br>
(On Windows, the command is probably called JLink rather than JLinkExe)<br>
At the JLink prompt, type<br>
&nbsp;&nbsp;`loadfile build/neodk_g071.hex`<br>
&nbsp;&nbsp;`r`<br>
&nbsp;&nbsp;`g`<br>
NeoDK's blue LED should now light up.
In another window, open a Segger console<br>
&nbsp;&nbsp;`JLinkRTTClient`<br>
This should now show output from NeoDK.<br>
In the JLinkRTTClient window, type<br>
&nbsp;&nbsp;`/?`<br>
to see a list of available NeoDK interactive commands.
