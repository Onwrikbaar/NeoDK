### ARM gcc
1. Download and install the [Arm GNU Toolchain](https://developer.arm.com/Tools%20and%20Software/GNU%20Toolchain) for bare metal targets.
2. Edit [Makefile.posix](toolchain/gcc/Makefile.posix) or [Makefile.windows](toolchain/gcc/Makefile.windows) (depending on your operating system) to set three environment variables to their appropriate values. Alternatively, they can be set in the startup file for your command line interface.

### STM32Cube
1. Download and install [STM32Cube for STM32G0 series](https://www.st.com/en/embedded-software/stm32cubeg0.html).
2. In your home directory, create a softlink to directory 'STM32Cube' of the just installed tree.

### SEGGER J-Link
1. Download and install [Segger J-Link](https://www.segger.com/downloads/jlink/).
2. In the just installed Segger directory tree, find file JLink_V792k/Samples/RTT/SEGGER_RTT_V792k.tgz and unpack it in place.
3. In your home directory, create a softlink named 'SEGGER_RTT' to directory 'JLink_V792k/Samples/RTT/SEGGER_RTT_V792k/RTT' of the just installed tree.

### Checking your installation
In this directory ('firmware'), at the command prompt type

&nbsp;&nbsp;`make`

This should compile the demo code without errors and create file 'neodk_g071.hex' (and a few others) in directory 'build'.

### Flashing the MPU using Segger JLink
On the commandline type<br/>
&nbsp;&nbsp;`cd firmware`<br/>
&nbsp;&nbsp;`JLinkExe -device stm32g071kb -if SWD -speed 4000 -autoconnect 1`<br>
(On Windows, the command is probably called JLink rather than JLinkExe)<br>
At the JLink prompt, type<br>
&nbsp;&nbsp;`loadfile build/neodk_g071.hex`<br>
&nbsp;&nbsp;`r`<br>
&nbsp;&nbsp;`g`<br>
In another window, open a Segger console<br>
&nbsp;&nbsp;`JLinkRTTClient`<br>
This should now show output from NeoDK.<br>
In the JLinkRTTClient window, type<br>
&nbsp;&nbsp;`/?`<br>
to see a list of available NeoDK interactive commands.
