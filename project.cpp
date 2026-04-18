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



 // Initialize 32 integer and 32 floating-point registers to 0
int int_registers[32] = {0};
float float_registers[32] = {0};

enum class InstType { 
    //overall R type:
    R_TYPE, //Standard Arithmetic (ADD, SUB, MUL, AND, ORR), R[Rd] = R[Rn] + R[Rm]
    SHIFT_TYPE, //Shift (LSL, LSR) R[Rd] = R[Rn] << R[Rm]
    FP_TYPE, //Floating Point (FADDD, FSUBD, FMULD)
    
    I_TYPE, //Immediate (ADDI, SUBI, ANDI, ORRI) R[Rd] = R[Rn] + Immediate
    IM_TYPE, //Immediate with shift (MOVZ, MOVK)
    D_TYPE, //Data transfer (STUR, LDUR, LDURD, STURD)
    CB_TYPE, //Conditional Branch (CBZ, CBNZ)
    B_TYPE, //Branch (B)
    HALT_TYPE, //HALT (HLT)
};

//vector and string constanta to organize each type. Used for parsing instructions
const vector<string> r_type = {"ADD", "SUB", "MUL", "AND", "ORR"};
const vector<string> shift_type = {"LSL", "LSR"};
const vector<string> fp_type = {"FADDD", "FSUBD", "FMULD"};

const vector<string> i_type = {"ADDI", "SUBI", "ANDI", "ORRI"};
const vector<string> im_type = {"MOVZ", "MOVK"};
const vector<string> d_type = {"STUR", "LDUR", "LDURD", "STURD"};
const vector<string> cb_type = {"CBZ", "CBNZ"};
const string B_TYPE = "B";
const string HALT_TYPE = "HLT";


//structure of indivdual instruction
struct Instruction {

    string opcode;

    InstType type;

    int rd = -1;
    int rn = -1;
    int rm = -1;

    int shift_amount = -1; //for shift in IM instructions
  
    long long  immediate = 0;
    int address = -1; //branch target address for branch instructions
    string label = ""; //label for branch instructions

    // Execution State
    int latency = 1;         // IU/FP latency
    int cyclesRemaining = 0; // For multi-cycle EX
    long long intResult = 0;
    double fpResult = 0.0;

    //cycle components
    //to know the cycle each stage completes
    int if_start = 0;
    int if_end = 0;
    int id_cycle = 0;
    int ex_cycle = 0;
    int mem_cycle = 0;
    int wb_cycle = 0;

    //helper flags

    //helps if it uses r or d registers
    bool is_fp = false; // True for LDURD/STURD (needs 2 memory cycles)
};

//vector address = memory address / 4 (since each instruction is 4 bytes)
vector<Instruction> instruct_list; // Vector to hold the instructions read from the file

//store labels with line number for branch instructions
map<string, int> labels = {
        {"Test", 10},
    };

struct Instruction createInstruction(vector<string>& tokens);

void printInstructions();

void readInstructions(string filename);

bool checkOpcode(string opcode, vector<string> type_vector);


int main(){

    readInstructions("inst.txt");
    printInstructions();

    return 0;
}

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


        // Remove comments from the line
        size_t pos = line.find(';');

        if (pos != string::npos) {
            line = line.substr(0, pos); // keep only before ;
        }

        //turn lowercase labels into uppercase for easier parsing
        for (char& c : line) {
            c = toupper(c);
        }

        //increase instruction count for each line read, used for labels
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

         //push back intruction when done
         instruct_list.push_back(createInstruction(tokens));


    }
}

void printInstructions(){

    for(const auto& instr : instruct_list){
        // Print each instruction to the console
        cout << "Instruction: " << instr.opcode 
        << " immediate: " << instr.immediate 
        << " rd: " << instr.rd 
        << " rn: " << instr.rn 
        << " rm: " << instr.rm 
        << " type: " << static_cast<int>(instr.type)
        << endl;
    }
}


struct Instruction createInstruction(vector<string>& tokens){

    Instruction instr;

    string opcode = tokens[0];

    cout << "opcode is " << opcode << endl;

    instr.opcode = opcode;

    //check if its each type, then implement

    // R_type Arithmetic (ADD, SUB, MUL, AND, ORR), R[Rd] = R[Rn] + R[Rm]
    if (find(r_type.begin(), r_type.end(), opcode) != r_type.end()) {

        cout << "R type instruction found, opcode " << opcode << endl;


        instr.rd = stoi(tokens[1].substr(1)); // Rd
        instr.rn = stoi(tokens[2].substr(1)); // Rn
        instr.rm = stoi(tokens[3].substr(1)); // Rm


        instr.type = InstType::R_TYPE;

        if (opcode == "ADD" || opcode == "SUB"){
            instr.latency = 2; // 2 cycles for ADD and SUB
        }

        else if (opcode == "MUL"){
            instr.latency = 3; // 3 cycles for MUL
        }
        
        return instr;
    }
    
    //Shift Type, (LSL, LSR) R[Rd] = R[Rn] << R[Rm]
    else if (checkOpcode(opcode, shift_type)) {

        cout << "Shift type instruction found, opcode " << opcode << endl;

        instr.rd = stoi(tokens[1].substr(1)); // Rd
        instr.rn = stoi(tokens[2].substr(1)); // Rn
        instr.immediate = stoll(tokens[3].substr(1)); // Immediate value

        instr.type = InstType::SHIFT_TYPE;
    }

    //FP TYPE, Floating Point (FADDD, FSUBD, FMULD) R[Rd] = R[Rn] + R[Rm]
    else if (checkOpcode(opcode, fp_type)) {

        cout << "FP type instruction found, opcode " << opcode << endl;

        instr.rd = stoi(tokens[1].substr(1)); // Rd
        instr.rn = stoi(tokens[2].substr(1)); // Rn
        instr.rm = stoi(tokens[3].substr(1)); // Rm


        instr.type = InstType::FP_TYPE;

        if (opcode == "FADDD" || opcode == "FSUBD"){
            instr.latency = 3; // 3 cycles for FADDD and FSUBD
        }

        else if (opcode == "FMULD"){
            instr.latency = 6; // 6 cycles for FMULD
        }
    } 


    //I_TYPE, Immediate (ADDI, SUBI, ANDI, ORRI) R[Rd] = R[Rn] + Immediate    
    else if (checkOpcode(opcode, i_type)) {

        cout << "I type instruction found, opcode " << opcode << endl;

        instr.rd = stoi(tokens[1].substr(1)); // Rd
        instr.rn = stoi(tokens[2].substr(1)); // Rn
        instr.immediate = stoll(tokens[3].substr(1)); // Immediate value

        instr.type = InstType::I_TYPE;

        if (opcode == "ADDI" || opcode == "SUBI"){
            instr.latency = 2; // 2 cycles for ADDI and SUBI
        }
    } 


    //Immediate with shift (IM Type) (MOVZ, MOVK)  
    else if (checkOpcode(opcode, im_type)) {

        cout << "IM type instruction found, opcode " << opcode << endl;

        instr.rd = stoi(tokens[1].substr(1)); // Rd
        instr.immediate = stoll(tokens[2].substr(1)); // Immediate value
        instr.shift_amount = stoll(tokens[4]); // Shift amount

        instr.type = InstType::IM_TYPE;
        
    } 

    //Data transfer (STUR, LDUR, LDURD, STURD) 
    else if (checkOpcode(opcode, d_type)) {
        
        cout << "D type instruction found, opcode " << opcode << endl;

        instr.rd = stoi(tokens[1].substr(1)); // Rd
        instr.rn = stoi(tokens[2].substr(2)); // rn
        instr.immediate = stoi(tokens[3].substr(1, tokens[3].length() - 1)); // immediate value

        instr.type = InstType:: D_TYPE;
        
        if (opcode == "LDURD" || opcode == "STURD"){
            instr.is_fp = true; 
        }
    }
    

    //CB_TYPE, //Conditional Branch (CBZ, CBNZ)
    else if (checkOpcode(opcode, cb_type)) {

        cout << "CB type instruction found, opcode " << opcode << endl;

        instr.rn = stoi(tokens[1].substr(1)); // Rn
        instr.label = tokens[2]; // Label for branch target

        instr.type = InstType:: CB_TYPE;
    } 

    //B_TYPE, //Unconditional Branch (B)
    else if (opcode == B_TYPE) {

        cout << "B type instruction found, opcode " << opcode << endl;

        instr.label = tokens[1]; // Label for branch target

        instr.type = InstType:: B_TYPE;
    }
    //HALT_TYPE, //HALT (HLT)
    else if (opcode == HALT_TYPE) {

        cout << "HALT type instruction found, opcode " << opcode << endl;

        instr.type = InstType:: HALT_TYPE;
    }

     else {
        cerr << "Error: Unknown opcode " << opcode << endl;
    }

     return instr; // Return the instruction struct (may be uninitialized if opcode is unknown)

    
}


bool checkOpcode(string opcode, vector<string> type_vector){

    if (find(type_vector.begin(), type_vector.end(), opcode) != type_vector.end()) {
        return true;
    } 

    return false;
}





