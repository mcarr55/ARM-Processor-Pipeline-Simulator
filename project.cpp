#include <iostream>
#include <fstream>
#include <vector>
#include <string>
using namespace std;


//The project executable should be named “simulator”.
//The format of the command line shall be as follows: 
//“simulator inst.txt data.txt output.txt”


//The simulator must output all desired information 
//to ONE output file “output.txt”.

//vector address = memory address / 4 (since each instruction is 4 bytes)
vector<string> instruct_list; // Vector to hold the instructions read from the file

enum class InstType { 
    R_TYPE, //Standard Arithemtic (ADD, SUB, MUL)
    I_TYPE, //Immediate (ADDI, SUBI)
    D_TYPE, //Data transfer (LDUR, STUR)
    B_TYPE, //Branch (B, CBZ, CBNZ)
    FP_TYPE, //Floating Point (FADD, FSUB, FMUL)
    HALT_TYPE, //HALT
    OTHER_TYPE  
    };

//structure of indivdual instruction
struct Instruction {

    string opcode;

    int rs;
    int rt;
    int rd;
    int shamt;
    int funct;
    int immediate;
    int address;

    //cycle components
    //to know the cycle each stage completes
    int if_cycle;
    int id_cycle;
    int ex_cycle;
    int mem_cycle;
    int wb_cycle;

    //helper flags

    //helps if it uses r or d registers
    bool is_fp;
};

void readInstructions(string filename){

    //Read from text file
    ifstream file(filename);

    string line;

    while(getline(file, line)){

        instruct_list.push_back(line); // Store the instruction in the vector
    }
}

void printInstructions(){

    for(const auto& instr : instruct_list){
        cout << instr << endl; // Print each instruction to the console
    }
}


int main(){

    readInstructions("inst.txt");
    printInstructions();

    return 0;
}






