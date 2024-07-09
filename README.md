# HCoEthernet
Some of the software/hardware developed throughout my HCoE final project. Work in progress.


custom_routine.c: a routine written to run on the Zynq 7020 PS and interface with the Ethernet controller using the driver. 

packet.c: Interfaces with Zynq SoC over Ethernet using the Linux packet API

transform_unit.v: heart of FPGA implementation, attempts to calculate int2int 2D wavelet transform in hardware.

test.c: Attempts to calculate int2int 2D wavelet transform in software.

thread_test.c: Basic pthread synchronization test.

bpf_compiler: compiles bpf filter programs using the npcap library from a filter string like the one we'd use with wireshark.

