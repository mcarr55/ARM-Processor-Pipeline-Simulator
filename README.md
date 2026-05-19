Program overview


This program is a c++ made 5-stage pipeline simulator to simulate an arm processor layout.


It reads and performs the instruction files in inst.txt, loads the values from data.txt into memory, and outputs it to output.txt.


Known issues


I know that after a program finishes its cycles in the EX stage, the instruction is said to leave the stage at that particular cycle. In my simulation, it is updated to the cycle it is leaving the EX stage when MEM is available to enter.


Instructions for use:


To run the program you can use the makefile's make command in the directory to compile the program with the make file.


After making, type "./simulator inst.txt data.txt output.txt" to run, with the instruction file, data file, and output file as inputs.


Supported Instructions:
Standard Arithmetic (ADD, SUB, MUL, AND, ORR)
Shifts: (LSL, LSR)
Immediate (ADDI, SUBI, ANDI, ORRI)
Immediate with shift (MOVZ, MOVK)
Data transfer (STUR, LDUR, LDURD, STURD)
Conditional Branch (CBZ, CBNZ)
Branch (B)
HALT (HLT)





