# CMSC 411, Fall 2025, Term project Makefile

simulator:
	gcc project.c -o simulator
clean:
	rm simulator *.o core*