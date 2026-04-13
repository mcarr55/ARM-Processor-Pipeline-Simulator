#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <map>
using namespace std;


//The project executable should be named “simulator”.
//The format of the command line shall be as follows: 
//“simulator inst.txt data.txt output.txt”


//The simulator must output all desired information 
//to ONE output file “output.txt”.

//vector address = memory address / 4 (since each instruction is 4 bytes)
vector<string> instruct_list; // Vector to hold the instructions read from the file

 // Initialize 32 integer and 32 floating-point registers to 0
int int_registers[32] = {0};
float float_registers[32] = {0};

enum class InstType { 
    R_TYPE, //Standard Arithmetic (LDURD, STURD, ADD, MUL, AND, ORR, LSL, LSR, FADDD, FSUBD, FMULD), R[Rd] = R[Rn] + R[Rm]
    I_TYPE, //Immediate (ADDI, SUBI, ANDI, ORRI) R[Rd] = R[Rn] + Immediate
    IM_TYPE, //Immediate with shift (MOVZ, MOVK)
    D_TYPE, //Data transfer (STUR, LDUR)
    CB_TYPE, //Branch (CBZ, CBNZ)
    B_TYPE, //Branch (B)
    FP_TYPE, //Floating Point 
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

//store labels with line number for branch instructions
map<string, int> labels = {
        {"Test", 10},
    };

void readInstructions(string filename){

    //Read from text file
    ifstream file(filename);

    string line;
    stringstream ss(line);



    int instruction_count = 0; // Counter to keep track of the number of instructions read

   
    //parse each individual instruction and store in vector
    while(getline(file, line)){

        // Replace commas with spaces for easier parsing
        replace(line.begin(), line.end(), ',', ' '); 

        instruction_count += 1;

        //get indivdual line
        stringstream ss(line);

        vector<string> tokens; 


        //parse within the line        
        while(ss >> line){

            //check for label, store if it exists and remove from tokens
            if(line.back() == ':'){

                //get label without the colon and store with line number
                string curr_label = line.substr(0, line.length() - 1);

                labels[curr_label] = instruction_count;

                cout << "label found, line " << labels[curr_label] << ", name " << curr_label << endl;

            }

            else{           
                //line is each indivdual word
                tokens.push_back(line);

            }

            
        }

        

         cout << "printing first token " << tokens[0] << endl;
         cout << "printing 2nd token " << tokens[1] << endl;

    }
}

void printInstructions(){

    for(const auto& instr : instruct_list){
        cout << instr << endl; // Print each instruction to the console
    }
}


int main(){

    readInstructions("inst.txt");
    //printInstructions();

    return 0;
}


int detectType(string opcode){

    enum InstType {R_TYPE, I_TYPE, IM_TYPE, D_TYPE, CB_TYPE, B_TYPE, FP_TYPE, HALT_TYPE, OTHER_TYPE};

    vector<string> r_type = {"ADD", "MUL", "AND", "ORR", "LSL", "LSR", "FADDD", "FSUBD", "FMULD"};
    vector<string> i_type = {"ADDI", "SUBI", "ANDI", "ORRI"};
    vector<string> im_type = {"MOVZ", "MOVK"};
    vector<string> d_type = {"STUR", "LDUR"};
    vector<string> cb_type = {"CBZ", "CBNZ"};
    vector<string> b_type = {"B"};

    if (find(r_type.begin(), r_type.end(), opcode) != r_type.end()) {
        return R_TYPE;
    } 

    else if (find(i_type.begin(), i_type.end(), opcode) != i_type.end()) {
        return I_TYPE;
    } 

    else if (find(im_type.begin(), im_type.end(), opcode) != im_type.end()) {
        return IM_TYPE;
    } 

    else if (find(d_type.begin(), d_type.end(), opcode) != d_type.end()) {
        return D_TYPE;
    } 

    else if (find(cb_type.begin(), cb_type.end(), opcode) != cb_type.end()) {
        return CB_TYPE;
    } 

    else if (find(b_type.begin(), b_type.end(), opcode) != b_type.end()) {
        return B_TYPE;
    }

    else{
        return OTHER_TYPE;
    }

    
}






