#include <iostream>
#include <fstream>
using namespace std;


//The project executable should be named “simulator”.
//The format of the command line shall be as follows: 
//“simulator inst.txt data.txt output.txt”


//The simulator must output all desired information 
//to ONE output file “output.txt”.

void readInstructions(string filename){

    //Read form text file
    ifstream file(filename);

    string line;

    while(getline(file, line)){

        cout << line << endl; // For testing, print the instruction
    }
}


int main(){

    readInstructions("inst.txt");

    return 0;
}






