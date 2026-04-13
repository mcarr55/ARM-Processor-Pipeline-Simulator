# CMSC 411, Fall 2025, Term project Makefile

TARGET = simulator

CXX = g++
CXXFLAGS = -g -Wall -std=c++17

all:
	g++ $(CXXFLAGS) project.cpp -o $(TARGET)
clean:
	rm -f $(TARGET)