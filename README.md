# NeoDK
Advanced electrostimulation machine development kit.

## Overview
### What is it?
This project describes the design and implementation of a bare-bones, yet highly innovative and powerful, pulse-based electrostimulation device. Both its hardware and its firmware are open for modification and extension.

### Author
Discord: @Onwrikbaar<br/>
E: <onwrikbaar@hotmail.com>

### Status
This is a work in progress. The electronics are production-ready. The firmware is far from finished.

### Goals
1. The last quarter century, innovation in the e-stim realm has been virtually nonexistent, despite massive advances in electronics and cheap computing power. Currently available commercial devices are without exception channel-oriented and produce TENS-style waveforms. Most are horrendously energy-inefficient. The time has come to unlock the potential of a more capable architecture.
2. It is fairly easy for non-programmers to create, share and improve routines to be played on stereostim setups. Currently there is no way to achieve the same for pulse-based e-stim devices. This project aims to (eventually) bridge that gap.

### Disclaimer
NeoDK is experimental, recreational, educational hardware and software. It comes with absolutely no warranty of any kind. Its designs are free to use - under the conditions set forth in its [Licence](LICENSE.txt). Using the designs -in original or modified form- implies acceptance of any and all associated risks and liabilities.

### How to use NeoDK
- Use it entirely at your own risk.
- Keep any and all electrodes connected to this device placed below the waist.
- Always start out at low intensity, especially when using insertable electrodes.

### How NOT to use NeoDK
- Do not use it for anything safety-critical, medical, industrial, military, commercial, immoral, or illegal - it was not designed for any of that.
- Do not use it on anyone without explicit and informed consent.
- Do not use a so-called triphase cable with this device!

### Safety
NeoDK can generate potentially lethal voltages and currents on its outputs and on internal components, even when powered from a battery. The risks associated with building, testing and using this device can be reduced but not eliminated.

The NeoDK device is designed to stay within the IEC 60601-2-10 limits once its firmware is out of beta. However, these limits are not necessarily maintained when the supply voltage, circuit, components or accompanying firmware are changed in any way.

### Audience
This project is intended for experienced electronics hobbyists and professionals, who possess a keen understanding of the risks associated with electrostimulation and high-voltage circuitry, and who desire one or more of the following:
- e-stim sensations unattainable through conventional power boxes.
- a high-intensity remotely controllable e-stim device to incorporate into a multi-toy setup or home control system.
- a deeper understanding of advanced, energy-efficient electronic circuitry for pulse-based electrostimulation.
- an innovative yet compact, proven design to base one's own developments on.
- a pulse-based device that can store and play user-created routines (planned for 2025).

### Caveat
NeoDK is _NOT_ a ready-for-play e-stim box. It is a minimal viable design (MVD) of a powerful and highly efficient electrostimulation device that is not limited by 2-pole 'channels' or strictly TENS-like waveforms. Schematic and board CAD files are included, but to get a working device involves some soldering. To get more than the basic functionality requires programming in C.

### How to get a working NeoDK
By far the easiest way to build NeoDK, is to order the SMD-populated PCB from JLCPCB (using the files in directory JLCPCB_prod) and buy the six through hole components separately. Soldering the through hole components onto the board is quite easy.

### Licensing
NeoDK's electronics and firmware can do things no commercially available e-stim box comes close to. If this appeals to you but you would like to have more features than NeoDK offers, by all means use NeoDK as the foundation for your own design - while observing this project's [Licence](LICENSE.txt). Regarding licensing for commercial / non-open source purposes, please contact the author.

### Why to start simple
"A complex system that works is invariably found to have evolved from a simple system that worked. A complex system designed from scratch never works and cannot be patched up to make it work. You have to start over with a working simple system." -- John Gall

## NeoDK in the media
Joanne's Reviews' always informative and entertaining YouTube [E-stim Livestreams](https://www.youtube.com/@JoannesReviews/streams) recently featured an [item about NeoDK](https://youtu.be/giEaDksRmh0?t=1460).