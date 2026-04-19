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
 // index 31 is the zero register, always returns 0 when read and discards writes
int int_registers[32] = {0};
float float_registers[32] = {0};

//Initialized from the 1s and 0s in data.txt
int data_memory[32] = {0};


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

    string opcode = "";

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


    //values read from registers during decode, used for execution
    int32_t val1 = 0;   // value read from register Rn
    int32_t val2 = 0;   // value read from register Rm or the immediate
    int32_t result = 0; // final answer from values

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

//store instructions in cycle, keep count with PC and cycle count
Instruction IF_ID_latch;
Instruction ID_IU1_latch;
Instruction IU1_IU2_latch;
Instruction IU2_IU3_latch;
Instruction IU3_MEM_latch;
Instruction MEM_WB_latch;

int PC = 0; 
int cycle = 0;











//for the datafile
void loadData(string filename);

void testData();

//for the parsrer
struct Instruction createInstruction(vector<string>& tokens);

void printInstructions();

void readInstructions(string filename);

bool checkOpcode(string opcode, vector<string> type_vector);

//for the simulator

// 5. Writeback (WB) - Update registers
void writeBack();

// 4. Memory (MEM) - Read/Write data_memory
void memory();

// 3. Execute (EX) - Math and Logic
void execute();

// 2. Decode (ID) - Read Register File
void decode();

// 1. Fetch (IF) - Get next instruction
void fetch(vector<Instruction>& program);



//load and store data from data.txt into data_memory array
void loadData(string filename){

    //Read from text file
    ifstream file(filename);

    string line;

    int i = 0;
    while (getline(file, line) && i < 32) {

        // Remove any stray carriage returns (\r) if the file was made on Windows
        if (!line.empty() && line.back() == '\r') line.pop_back();
        
        if (line.length() >= 32) {
            // stoul converts the 32-bit binary string to an integer
            // The '2' tells it to interpret it as Base-2 (Binary)
            data_memory[i] = (int32_t)stoul(line, nullptr, 2);
            i++;
        }
    }
    file.close();
}

void testData(){
    for(int j = 0; j < 5; j++) {
        cout << "Memory Address " << 256 + (j*4) << ": " << data_memory[j] << endl;
    }
}




int main(){

    //loadData("data.txt");
    //testData();
    readInstructions("inst.txt");
    printInstructions();
    
   
   int_registers[2] = 100; //temp test value
   //int_registers[3] = 50;

    //create a loop to run the simulator until we hit a HALT instruction or run out of instructions



    //the loop runs backwards to have the stages increment foward
    //once each time in the while loop
    while (true) {

        cycle++;

        //move info foward in the pipeline, 

        //Move IU instuction once each stage
        MEM_WB_latch  = IU3_MEM_latch;
        IU3_MEM_latch = IU2_IU3_latch;
        IU2_IU3_latch = IU1_IU2_latch;

        // This moves the instruction that execute() just worked on
        IU1_IU2_latch = ID_IU1_latch; 
    
        // This moves the instruction that decode() just worked on
        ID_IU1_latch  = IF_ID_latch;

        //backwards to 
        // 3. Writeback (WB)

        // 2. Memory (MEM)
        memory();

        // 2. Execute (EX)
        execute();

        // 1. Decode (ID)
        decode();

        // 0. Fetch (IF)
        fetch(instruct_list);




        //debug for fetch
        cout << "fetch_debug" << endl;
        cout << "Cycle " << cycle << " | IF_ID holds: " << IF_ID_latch.opcode << endl;
        cout << "Cycle " << cycle << " | ID_IU1 holds: " << ID_IU1_latch.opcode << endl;
        cout << "-----------------------------------" << endl;
       
        



        //debug for decode
        cout << "decode_debug" << endl;
        cout << "Cycle " << cycle << ":" << endl;
        cout << "  Fetch Stage (IF_ID): " << IF_ID_latch.opcode << endl;
        cout << "  Decode Stage (ID_IU1): " << ID_IU1_latch.opcode 
        << " | Val1: " << ID_IU1_latch.val1 
        << " | Val2: " << ID_IU1_latch.val2 << endl;
        cout << "-----------------------------------" << endl;
  

        //debug for result of execution
        cout << "execute_debug" << endl;
        cout << "Cycle " << cycle << ":" << endl;
        cout << "  Execute Stage (IU1_IU2): " << IU1_IU2_latch.opcode 
        << " | Result: " << IU1_IU2_latch.result << endl;
        cout << "-----------------------------------" << endl;
        cout << endl;
    

        /*
        cout << "--- Cycle " << cycle << " ---" << endl;
        cout << "ID_IU1_latch Opcode: " << ID_IU1_latch.opcode << endl;
        cout << "ID_IU1_latch Val1 (Rn): " << ID_IU1_latch.val1 << endl;
        cout << "ID_IU1_latch Val2 (Rm/Imm): " << ID_IU1_latch.val2 << endl;
        cout << "----------------------" << endl;
        */

        /*
        if (IU1_IU2_latch.opcode != "EMPTY") {
            cout << "Execute Stage | Op: " << IU1_IU2_latch.opcode 
            << " | " << IU1_IU2_latch.val1 << " " << IU1_IU2_latch.opcode << " " << IU1_IU2_latch.val2 
            << " = " << IU1_IU2_latch.result << endl;
        }
        */

        /*
        // Inside your while loop, after all functions and movements:
        cout << "CYCLE " << cycle << " DEBUG:" << endl;
        cout << "  1. IF_ID   Op: " << IF_ID_latch.opcode << endl;
        cout << "  2. ID_IU1  Op: " << ID_IU1_latch.opcode << " | V1: " << ID_IU1_latch.val1 << " | V2: " << ID_IU1_latch.val2 << endl;
        cout << "  3. IU1_IU2 Op: " << IU1_IU2_latch.opcode << " | Res: " << IU1_IU2_latch.result << endl;
        */

        /*
        // --- FINAL PIPELINE SNAPSHOT ---
        cout << "================ CYCLE " << cycle << " ================" << endl;
        cout << " [IF]  Latch: " << IF_ID_latch.opcode << endl;

        cout << " [ID]  Latch: " << ID_IU1_latch.opcode 
            << " | V1: " << ID_IU1_latch.val1 
            << " | V2: " << ID_IU1_latch.val2 << endl;

        cout << " [EX]  Latch: " << IU1_IU2_latch.opcode 
            << " | Result: " << IU1_IU2_latch.result << endl;

        cout << " [MEM] Latch: " << IU3_MEM_latch.opcode 
            << " | Result: " << IU3_MEM_latch.result << endl;

        cout << " [WB]  Latch: " << MEM_WB_latch.opcode 
            << " | Writing " << MEM_WB_latch.result << " to R" << MEM_WB_latch.rd << endl;
        cout << "==========================================" << endl << endl;
        */

        // A temporary break so we don't loop forever while testing
        if (cycle > 10) break; 
}

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

    file.close();
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

//look at the PC, grab the current instruction from the instruction vector, put it into the IF_ID_latch.
void fetch(vector<Instruction>& program){

    // If the PC is within the bounds of our instruction vector
    if (PC / 4 < program.size()) {

        IF_ID_latch = program[PC / 4];

        // Move to the next instruction address
        PC += 4; 
    } 
    
    else {

        // If we ran out of instructions, put an "Empty" one in the latch
        IF_ID_latch = Instruction(); 
        IF_ID_latch.opcode = "";
    }
}

//look up values in register file, put them in the ID_IU1_latch for the next stage
void decode(){

    // grab the register values If it's a real instruction or not EMPTY, 
    if (IF_ID_latch.opcode != "") {

        // Read the first source register value

        // XZR should always return 0
        if (ID_IU1_latch.rn == 31) {
            ID_IU1_latch.val1 = 0;
        }

        else{
            // Example: If the instruction uses 'rn' (first source register)
            // We look up its value in our physical register array      
           ID_IU1_latch.val1 = int_registers[ID_IU1_latch.rn];
        }

        //Second source register or immediate value

         // For ADDI or SUBI, use immediate is already in struct.
        if (ID_IU1_latch.opcode == "ADDI" || ID_IU1_latch.opcode == "SUBI" || 
            ID_IU1_latch.opcode == "LDUR" || ID_IU1_latch.opcode == "STUR" ||
            ID_IU1_latch.opcode == "LSL" || ID_IU1_latch.opcode == "LSR" ||
            ID_IU1_latch.opcode == "ANDI" || ID_IU1_latch.opcode == "ORRI") {
                
            ID_IU1_latch.val2 = ID_IU1_latch.immediate;
        }

        else if (ID_IU1_latch.rn == 31) {
            ID_IU1_latch.val2 = 0;
        }

        else{
            ID_IU1_latch.val2 = int_registers[ID_IU1_latch.rm];
        }

    }
}


void execute(){
    // IU1 reads from the latch filled by Decode

    //cout << "opcode in execute is " << ID_IU1_latch.opcode << endl;

    if (IU1_IU2_latch.opcode != "") {
        string op = IU1_IU2_latch.opcode;

        if (op == "ADD" || op == "ADDI") {
            IU1_IU2_latch.result = IU1_IU2_latch.val1 + IU1_IU2_latch.val2;
            cout << "  [ALU] Math Check: " << IU1_IU2_latch.val1 << " + " << IU1_IU2_latch.val2 << " = " << IU1_IU2_latch.result << endl;
        } 
        else if (op == "SUB" || op == "SUBI") {
            IU1_IU2_latch.result = IU1_IU2_latch.val1 - IU1_IU2_latch.val2;
        }
        else if (op == "AND" || op == "ANDI") {
            IU1_IU2_latch.result = IU1_IU2_latch.val1 & IU1_IU2_latch.val2;
        }
        else if (op == "ORR" || op == "ORRI") {
            IU1_IU2_latch.result = IU1_IU2_latch.val1 | IU1_IU2_latch.val2;
        }

        // Shifts (LSL/LSR)
        else if (op == "LSL") {
            IU1_IU2_latch.result = (uint32_t)IU1_IU2_latch.val1 << IU1_IU2_latch.val2;
        }
        else if (op == "LSR") {
            IU1_IU2_latch.result = (uint32_t)IU1_IU2_latch.val1 >> IU1_IU2_latch.val2;
        }
    }
    else{

        //cout << "  [ALU] Invalid instruction" << endl;

    }
    

}

void memory(){
   // MEM works on the instruction in the IU2_IU3 latch
    if (IU2_IU3_latch.opcode != "EMPTY") {
        
        string op = IU2_IU3_latch.opcode;
        int address = IU2_IU3_latch.result; // The address calculated in EX

        if (op == "LDUR") {
            // Read from memory into the result field
            IU2_IU3_latch.result = data_memory[address];
        } 
        else if (op == "STUR") {
            // Write the value of the source register into memory
            // Note: For STUR, 'val3' or 'rt' is the data to be stored
            data_memory[address] = int_registers[IU2_IU3_latch.rd];
        }
    }
}

void writeback() {
    // WB works on the instruction in the very last latch
    if (MEM_WB_latch.opcode != "EMPTY") {
        
        string op = MEM_WB_latch.opcode;

        // Instructions that write to a register
        if (op == "ADD" || op == "ADDI" || op == "SUB" || op == "SUBI" || 
            op == "AND" || op == "ORR"  || op == "LSL" || op == "LSR" || 
            op == "LDUR") {
            
            // Safety check: Never write to R31 (the zero register)
            if (MEM_WB_latch.rd != 31 && MEM_WB_latch.rd != -1) {
                int_registers[MEM_WB_latch.rd] = MEM_WB_latch.result;
            }
        }
    }
}

//When testing your WB stage, try printing your registers at the very end of each cycle. 
//You will see that even though ADDI shows up in the trace at Cycle 1, the int_registers[rd] 
//value won't change until the end of Cycle 4 or beginning of Cycle 5.

