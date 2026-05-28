# Program overview:


This program simulates the 5-stage pipeline design of an ARM Processor layout using c++.

It reads and performs the instruction files in inst.txt, loads the values from data.txt into memory, and outputs it to output.txt.



## Process of Program:

An instruction file written in assembly is loaded from inst.txt

The values of memory represnted from binary 0s and 1 are loaded from data.txt.

Based on the intructions, they are loaded into Pipeline.


- Intruction fetch: Enters the next intruction into the piepline.

- Decode: Identifires needed values and registers that correspond to an intruction.

- Execute: Performs an operation the intruciton does like adding for example.

- Memory: For STUR intructions stores the value in the desitnation address in memory.  

- Writeback: Updates the vaue of the desitnation register as specified by the intruction. 


At the end of the program after 2 HLT instruciotns, the program ends and the cucle each intruciton exited each phaase is lisited. As well as the requests and successfult hits from the instruction and data caches.


## Known issues:


I know that after a program finishes its cycles in the EX stage, the instruction is said to leave the stage at that particular cycle. In my simulation, it is updated to the cycle it is leaving the EX stage when MEM is available to enter.


## Instructions for use:


To run the program you can use the makefile's make command in the directory to compile the program with the make file.


After making, type "./simulator inst.txt data.txt output.txt" to run, with the instruction file, data file, and output file as inputs.


### Supported Instructions:

Standard Arithmetic (ADD, SUB, MUL, AND, ORR)
Shifts: (LSL, LSR)
Immediate (ADDI, SUBI, ANDI, ORRI)
Immediate with shift (MOVZ, MOVK)
Data transfer (STUR, LDUR, LDURD, STURD)
Conditional Branch (CBZ, CBNZ)
Branch (B)
HALT (HLT)





