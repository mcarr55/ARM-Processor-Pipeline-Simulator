#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <map>
#include <iomanip>
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

    int shift_amount = 0; //for shift in IM instructions
  
    long long immediate = 0;
    int address = -1; //branch target address for branch instructions
    string label = ""; //label for branch instructions

    // Execution State
    int latency = 1;         // IU/FP latency
    int cyclesRemaining = 0; // For multi-cycle EX
    long long intResult = 0;
    double fpResult = 0.0;

    //cycle components
    //to know the cycle each stage completes
    int if_exit = -1;
    int id_exit = -1;
    int ex_exit = -1;
    int mem_exit = -1;
    int wb_exit = -1;


    //values read from registers during decode, used for execution
    int32_t val1 = 0;   // value read from register Rn
    int32_t val2 = 0;   // value read from register Rm or the immediate
    int32_t result = 0; // final answer from values

    string raw_line= "";

    int number_id = -1;


    //helper flags

    //helps if it uses r or d registers
    bool is_fp = false; // True for LDURD/STURD (needs 2 memory cycles)

    bool regWrite = false;
};

//cache blocks:
int icache_miss_delay = 0;
bool final_hlt_fetch_started = false; // To trigger the last 12-cycle miss
int i_req = 0, i_hit = 0;

struct ICacheLine {
    bool valid = false;
    int tag = -1;
};
ICacheLine i_cache[4]; // 4 blocks (standard for this project)

// Help the simulator know how to split the address
struct ICacheParts { int tag; int index; };
ICacheParts splitICache(int address) {
    ICacheParts p;
    p.index = (address >> 4) & 0x3; // Bits 4-5 for index
    p.tag = (address >> 6);        // Bits 6+ for tag
    return p;
}

//global variables
bool icache_access_in_progress = false; 
bool memory_busy_with_dcache = false;

// Statistics
int icache_accesses = 0;

struct DCacheBlock {
    bool valid = false;
    bool dirty = false;
    uint32_t tag = 0;
    uint32_t words[4];
    int lru_bit = 0; // For 2-way, a simple 0 or 1 is enough to track "Last Used"
};

DCacheBlock d_cache[2][2];

struct DCacheParts {
    uint32_t tag;
    uint32_t index;
    uint32_t wordOffset;
};

DCacheParts splitDCache(uint32_t addr) {
    DCacheParts p;
    // Bits 0-1: Byte Offset (ignored)
    // Bits 2-3: Word Offset (same as I-Cache)
    p.wordOffset = (addr >> 2) & 0x3; 
    
    // Bit 4: Index (chooses Set 0 or Set 1)
    p.index = (addr >> 4) & 0x1;      
    
    // Bits 5-31: Tag
    p.tag = (addr >> 5);              
    
    return p;
}




//memory controller logic
int memory_busy_counter = 0; // Cycles remaining for current memory task
bool memory_serving_icache = false;
bool memory_serving_dcache = false;

// Statistics
int icache_hits = 0, icache_misses = 0;
int dcache_hits = 0, dcache_misses = 0;

int d_req = 0;
int d_hit = 0;





//vector address = memory address / 4 (since each instruction is 4 bytes)
vector<Instruction> instruct_list; // Vector to hold the instructions read from the file

//store labels with line number for branch instructions
map<string, int> labels = {
        {"Test", 10},
    };

//store instructions in cycle, keep count with PC and cycle count
Instruction IF_ID_register;
Instruction ID_IU1_register;
Instruction IU1_IU2_register;
Instruction IU2_IU3_register;
Instruction IU3_MEM_register;
Instruction MEM_WB_register;

Instruction post_WB_register_info;




int PC = 0; 
int cycle = 0;
bool activeHalt = false;

bool loadStall = false;






vector<string> line_list; // Vector to hold string of instructions read from the file



bool busy_iu; //if the iu is currently busy with an operation, other pars of the code cant write

bool is_final_miss = false; // Tracks if we are in the 12-cycle wait for the second HLT




//for the datafile
void loadData(string filename);

void testData();

//for the parsrer
struct Instruction createInstruction(vector<string>& tokens);

void printInstructions();

void readInstructions(string filename);

bool checkOpcode(string opcode, vector<string> type_vector);

//simulator pipeline funcitons
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

void printOutputFile(string filename);


void debugFunction();

void passTime(Instruction newIntruct, Instruction oldIntruct);

void printExits(Instruction instruct);


void executeHazardCheck();



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

    loadData("data.txt");
    testData();
    readInstructions("inst.txt");
    printInstructions();

   
    
   /*
   For one cycle test
   int_registers[2] = 25; //temp test value
   int_registers[3] = 19;

    int_registers[5] = 3;

    int_registers[6] = 9;
    int_registers[7] = 2;
    int_registers[8] = 10;
    int_registers[9] = 2;

    int_registers[10] = 16;
    */


    
    /*
    Multicycle test
    int_registers[2] = 2;
    int_registers[3] = 3;

    int_registers[5] = 20;
    int_registers[6] = 2;

    int_registers[8] = 2;

    int_registers[9] = 15;
    */

    int_registers[1] = 0;

    //int_registers[3] = 6;
   





    for (std::size_t i = 0; i < instruct_list.size(); ++i) {
        instruct_list[i].raw_line =  line_list[i];
        instruct_list[i].number_id = i;
    }

    //print intiial latency:
    cout << "init latency is: " <<  instruct_list[0].latency << endl;


    //create a loop to run the simulator until we hit a HALT instruction or run out of instructions



    //the loop runs backwards to have the stages increment foward
    //once each time in the while loop
    while (true) {

        cycle++;

        //backwards to 
        // 3. Writeback (WB)
        writeBack();

        //Move IU instuction once each stage
        //MEM_WB_register  = IU3_MEM_register;

        // 3. Memory (MEM)
        memory();

        // 2. Execute (EX)
        execute();

        if (busy_iu == false){
            // 1. Decode (ID)
            decode();

            if(loadStall == false){

                 // 0. Fetch (IF)
                fetch(instruct_list);


            }
        }

    // 2. Termination Logic:
    // We stop when the second HLT fetch (the "ghost" fetch) finishes its miss
    if (final_hlt_fetch_started && icache_miss_delay == 0) {
        break;
    }




        debugFunction();




        //move info foward in the pipeline, 

  


    

   
    


        loadStall = false;

        

        // A temporary break so we don't loop forever while testing
        if (cycle > 30 || activeHalt == true) break; 
    }

   

    printOutputFile("output.txt");

    /*
    cout << "int reg 1: " << int_registers[1] << endl;
    cout << "int reg 5: " << int_registers[5] << endl;
    cout << "int reg 6: " << int_registers[6] << endl;
    cout << "int reg 7: " << int_registers[7] << endl;
    cout << "int reg 8: " << int_registers[8] << endl;
    cout << "int reg 9: " << int_registers[9] << endl;
    cout << "int reg 10: " << int_registers[10] << endl;
    */

    /*
    //should be 5
    cout << "int reg 1: " << int_registers[1] << endl;

    //should be  18
    cout << "int reg 4: " << int_registers[4] << endl;

    //should be  6
    cout << "int reg 7: " << int_registers[7] << endl;

    //should be 12
    cout << "int reg 9: " << int_registers[9] << endl;
    */

    //should be 204
    cout << "int reg 1: " << int_registers[1] << endl;

    cout << "int reg 3: " << int_registers[3] << endl;



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


        // Remove comments from the line
        size_t pos = line.find(';');
        

        if (pos != string::npos) {
            line = line.substr(0, pos); // keep only before ;
        }

        //add individual lines to list
        line_list.push_back(line);

        // Replace commas with spaces for easier parsing
        replace(line.begin(), line.end(), ',', ' '); 

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

        instr.regWrite = true;
        
        return instr;
    }
    
    //Shift Type, (LSL, LSR) R[Rd] = R[Rn] << R[Rm]
    else if (checkOpcode(opcode, shift_type)) {

        cout << "Shift type instruction found, opcode " << opcode << endl;

        instr.rd = stoi(tokens[1].substr(1)); // Rd
        instr.rn = stoi(tokens[2].substr(1)); // Rn
        instr.immediate = stoll(tokens[3].substr(1)); // Immediate value

        instr.regWrite = true;

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

        instr.regWrite = true;
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

        instr.regWrite = true;
    } 


    //Immediate with shift (IM Type) (MOVZ, MOVK)  
    else if (checkOpcode(opcode, im_type)) {

        cout << "IM type instruction found, opcode " << opcode << endl;

        instr.rd = stoi(tokens[1].substr(1)); // Rd
        instr.immediate = stoll(tokens[2].substr(1)); // Immediate value
        instr.shift_amount = stoll(tokens[4]); // Shift amount

        instr.type = InstType::IM_TYPE;

        instr.regWrite = true;
        
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

        if (opcode == "LDUR" || opcode == "LDURD"){
            instr.regWrite = true;
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

     return instr;
}


bool checkOpcode(string opcode, vector<string> type_vector){

    if (find(type_vector.begin(), type_vector.end(), opcode) != type_vector.end()) {
        return true;
    } 

    return false;
}

//look at the PC, grab the current instruction from the instruction vector, put it into the IF_ID_register.
//look at the PC, grab the current instruction from the instruction vector, put it into the IF_ID_register.
void fetch(vector<Instruction>& program){

    // A: Handle active stalls
    if (icache_miss_delay > 0) {
        icache_miss_delay--;
        return; 
    }

    // If the PC is within the bounds of our instruction vector
    if (PC / 4 < program.size()) {
        ICacheParts p = splitICache(PC);
        i_req++;


        if (i_cache[p.index].valid && i_cache[p.index].tag == p.tag) {
            // CACHE HIT - Only here do we move the PC and record the exit
            i_hit++;

            IF_ID_register = program[PC / 4];

            IF_ID_register.if_exit = cycle;

            //update leaving value
            instruct_list[IF_ID_register.number_id].if_exit = IF_ID_register.if_exit;

            // Move to the next instruction address
            PC += 4; 
        } 
    
        else {

            // CACHE MISS - Start 12 cycle wait. Do NOT increment PC.
            icache_miss_delay = 11; // 12 cycles total including this one
            i_cache[p.index].valid = true;
            i_cache[p.index].tag = p.tag;
            // The function returns, and next cycle it will check the delay again.
        }
    }
    else if (!final_hlt_fetch_started) {
        i_req++;
        icache_miss_delay = 11; 
        final_hlt_fetch_started = true;
    }
}


//look up values in register file, put them in the ID_IU1_register for the next stage
void decode(){

    //cout << IF_ID_register.opcode << " is in decode stage" << endl;

    //case for Load data hazard, if intruction ui execute is ldur
    if (ID_IU1_register.opcode == "LDUR") {

        //check if current instruction need what will be loaded(
        if(IF_ID_register.rn == ID_IU1_register.rd || IF_ID_register.rm == ID_IU1_register.rd || IF_ID_register.rd == ID_IU1_register.rd){

            //if so stall for a cycle until it moves out, in which it would load the vaue by that point
            loadStall = true;

            ID_IU1_register = Instruction(); 
            ID_IU1_register.opcode = "";

            return;

        }
    }


    // grab the register values If it's a real instruction or not EMPTY, 
    if (IF_ID_register.opcode != "") {

        if(IF_ID_register.opcode == "HLT"){
            cout << "detecting halt operation" << endl;

            ID_IU1_register.id_exit = cycle;

            activeHalt = true;
        }

        ID_IU1_register.opcode = IF_ID_register.opcode;
        ID_IU1_register.number_id = IF_ID_register.number_id;

        // Read the first source register value

        // XZR should always return 0
        if (IF_ID_register.rn == 31) {
            ID_IU1_register.val1 = 0;
        }

        else if(ID_IU1_register.opcode == "MOVK"){
             ID_IU1_register.val1 = int_registers[IF_ID_register.rd];

        }

        else{
            // Example: If the instruction uses 'rn' (first source register)
            // We look up its value in our physical register array      
           ID_IU1_register.val1 = int_registers[IF_ID_register.rn];
        }
         //Second source register or immediate value

         // For ADDI or SUBI, use immediate is already in struct.
        if (IF_ID_register.opcode == "ADDI" || IF_ID_register.opcode == "SUBI" || 
            IF_ID_register.opcode == "LDUR" || IF_ID_register.opcode == "STUR" ||
            IF_ID_register.opcode == "LSL" || IF_ID_register.opcode == "LSR" ||
            IF_ID_register.opcode == "ANDI" || IF_ID_register.opcode == "ORRI" ||
            IF_ID_register.opcode == "MOVZ" || IF_ID_register.opcode == "MOVK") {
                
            ID_IU1_register.val2 = IF_ID_register.immediate;
        }

        else if (IF_ID_register.rn == 31) {
            ID_IU1_register.val2 = 0;
        }

        else{
            ID_IU1_register.val2 = int_registers[IF_ID_register.rm];
        }



        
        ID_IU1_register.if_exit = IF_ID_register.if_exit;
        ID_IU1_register.id_exit = cycle;

        ID_IU1_register.rd = IF_ID_register.rd;
        ID_IU1_register.rn = IF_ID_register.rn;
        ID_IU1_register.rm = IF_ID_register.rm;

        ID_IU1_register.latency = IF_ID_register.latency;
        ID_IU1_register.shift_amount = IF_ID_register.shift_amount;
        ID_IU1_register.label = IF_ID_register.label;

        ID_IU1_register.regWrite = IF_ID_register.regWrite;
         

        //update leaving value
        instruct_list[ID_IU1_register.number_id].id_exit = ID_IU1_register.id_exit;



    }
    else{
        // If we ran out of instructions, put an "Empty" one in the register
        ID_IU1_register = Instruction(); 
        ID_IU1_register.opcode = "";
    }
}


void execute(){
    // IU1 reads from the register filled by Decode

    //move 

    //check cases where ADD/SUB take longer in pipeline


    if (ID_IU1_register.opcode != "") {
    
        string op = ID_IU1_register.opcode;

        //check for data hazards
        executeHazardCheck();

        // --- LEVEL 3 (IU3) ---
        // MUL finally finishes its 3rd cycle here
        if (op == "MUL") {

            cout << "we are stalling " << op << endl;
            cout << ID_IU1_register.latency << endl;

            //check it for latency, if not ready stall 
            if(ID_IU1_register.latency > 1){

                busy_iu = true;
                ID_IU1_register.latency -= 1;

                //pass empty intruciton foward while stalling
                IU3_MEM_register = Instruction(); 
                IU3_MEM_register.opcode = "";
                return;
            }
            else{
                busy_iu = false;
            }

            //perform mul operation
            IU3_MEM_register.result = ID_IU1_register.val1 * ID_IU1_register.val2;
        } 

        // --- LEVEL 2 (IU2) ---
        else if (op == "ADD" || op == "ADDI" || op == "SUB" || op == "SUBI"){

            cout << "we are stalling " << op << endl;
            cout << ID_IU1_register.latency << endl;

            //check it for latency, if not ready stall 
            if(ID_IU1_register.latency > 1){

                busy_iu = true;
                ID_IU1_register.latency -= 1;

                //pass empty intruciton foward while stalling
                IU3_MEM_register = Instruction(); 
                IU3_MEM_register.opcode = "";
                return;
            }
            else{
                busy_iu = false;
            }


            // ADD and SUB finish their 2nd cycle here
            if (op == "ADD" || op == "ADDI") {
                IU3_MEM_register.result = ID_IU1_register.val1 + ID_IU1_register.val2;
                } 
            if (op == "SUB" || op == "SUBI") {
                IU3_MEM_register.result = ID_IU1_register.val1 - ID_IU1_register.val2;
            }
        }


        // --- LEVEL 1 (IU1) ---
        // Normal 1-cycle ops (AND, ORR, LSL) finish here
        else if (op == "AND" || op == "ANDI" || op == "ORR" || op == "ORRI" || op == "LSL" || op == "LSR"){

            if (op == "AND" || op == "ANDI") {

                IU3_MEM_register.result = ID_IU1_register.val1 & ID_IU1_register.val2;
            }

            else if (op == "ORR" || op == "ORRI") {

                IU3_MEM_register.result = ID_IU1_register.val1 | ID_IU1_register.val2;
            }

            // Shifts (LSL/LSR)
            else if (op == "LSL") {

                IU3_MEM_register.result = (uint32_t)ID_IU1_register.val1 << ID_IU1_register.val2;
            }

            else if (op == "LSR") {

                IU3_MEM_register.result = (uint32_t)ID_IU1_register.val1 >> ID_IU1_register.val2;
            }
        }

        //loading and storing
        if (op == "LDUR" || op == "STUR"){

            //calculate immedate address
            IU3_MEM_register.result = ID_IU1_register.val1 + ID_IU1_register.val2;
        }

        if( op == "MOVZ"){

            //shift
            IU3_MEM_register.result = ID_IU1_register.val2 << ID_IU1_register.shift_amount;
        }

        if(op == "MOVK"){

            uint32_t shift = ID_IU1_register.shift_amount * 16;
            uint32_t immediate = ID_IU1_register.val2;
            uint32_t currentVal1 = ID_IU1_register.val1;


            if (shift < 32) {
                uint32_t mask = ~(0xFFFF << shift);
                IU3_MEM_register.result = (currentVal1 & mask) | (immediate << shift);
                cout << "case 1 " << IU3_MEM_register.result << endl;
            } else {
                // If shift is 32 or more, we keep the whole thing as shift is out of bounds
                IU3_MEM_register.result = ID_IU1_register.val1;
                cout << "case 2 " << IU3_MEM_register.result << endl;
            }
        }

        if(op == "B"){

            // 2. Look up the line number in your Map
            // labels["LOOP"] would return something like line 5
            if (labels.find(ID_IU1_register.label) != labels.end()) {
                int targetLine = labels[ID_IU1_register.label];

                cout << "targetLine is " << targetLine << endl;

                //Update the PC to that line
                PC = targetLine*4 - 4;

                cout << "Branching to LABEL: " << ID_IU1_register.label << " at line " << targetLine << endl;

                //copy over other info
                
                //cycle info
                IU3_MEM_register.opcode = ID_IU1_register.opcode;
                IU3_MEM_register.if_exit = ID_IU1_register.if_exit;
                IU3_MEM_register.id_exit = ID_IU1_register.id_exit;
                IU3_MEM_register.ex_exit = cycle;
                

                IU3_MEM_register.rd = ID_IU1_register.rd;
                IU3_MEM_register.rn = ID_IU1_register.rn;
                IU3_MEM_register.rm = ID_IU1_register.rm;
                

                IU3_MEM_register.number_id = ID_IU1_register.number_id;
                IU3_MEM_register.regWrite = ID_IU1_register.regWrite;

                //Flush past intrucitons


                //Fetch
                IF_ID_register = Instruction(); 
                IF_ID_register.opcode = "";

                //Execute
                ID_IU1_register = Instruction(); 
                ID_IU1_register.opcode = "";

                return;
            } 
            else {
                cerr << "Error: Label '" << IU3_MEM_register.label << "' not found!" << endl;
            }
        }
        if(op == "CBZ" || op == "CBNZ"){

            // 2. Look up the line number in your Map
            // labels["LOOP"] would return something like line 5
            if (labels.find(ID_IU1_register.label) != labels.end()) {
                int targetLine = labels[ID_IU1_register.label];

                cout << "targetLine is " << targetLine << endl;

                //CBZ jump if zero
                //CBNZ jump if not zero
                if( (op == "CBZ" && ID_IU1_register.val1 == 0) || (op == "CBNZ" && ID_IU1_register.val1 != 0)){

                    //Update the PC to that line
                    PC = targetLine*4 - 4;
                 

                    cout << "Branching to LABEL: " << ID_IU1_register.label << " at line " << targetLine << endl;

                    //copy over other info
                    
                    //cycle info
                    IU3_MEM_register.opcode = ID_IU1_register.opcode;
                    IU3_MEM_register.if_exit = ID_IU1_register.if_exit;
                    IU3_MEM_register.id_exit = ID_IU1_register.id_exit;
                    IU3_MEM_register.ex_exit = cycle;
                    
                    IU3_MEM_register.rd = ID_IU1_register.rd;
                    IU3_MEM_register.rn = ID_IU1_register.rn;
                    IU3_MEM_register.rm = ID_IU1_register.rm;
                

                    IU3_MEM_register.number_id = ID_IU1_register.number_id;
                    IU3_MEM_register.regWrite = ID_IU1_register.regWrite;

                    //Flush past intrucitons


                    //Fetch
                    IF_ID_register = Instruction(); 
                    IF_ID_register.opcode = "";

                    //Execute
                    ID_IU1_register = Instruction(); 
                    ID_IU1_register.opcode = "";

                    return;

                }

                //dont banch but keep moving
                else{
                    cout << "branch is not taken" << endl;

                } 
            } 
            else {
                cerr << "Error: Label '" << IU3_MEM_register.label << "' not found!" << endl;
            }

        }
        //copy over other info
                    
        //cycle info
        IU3_MEM_register.opcode = ID_IU1_register.opcode;
        IU3_MEM_register.if_exit = ID_IU1_register.if_exit;
        IU3_MEM_register.id_exit = ID_IU1_register.id_exit;
        IU3_MEM_register.ex_exit = cycle;
                    
        IU3_MEM_register.rd = ID_IU1_register.rd;
        IU3_MEM_register.rn = ID_IU1_register.rn;
        IU3_MEM_register.rm = ID_IU1_register.rm;
                

        IU3_MEM_register.number_id = ID_IU1_register.number_id;

        IU3_MEM_register.regWrite = ID_IU1_register.regWrite;
        

        instruct_list[IU3_MEM_register.number_id].ex_exit = IU3_MEM_register.ex_exit;
    }


    else{
    
        IU3_MEM_register = Instruction(); 
        IU3_MEM_register.opcode = "";

    }
    

}

void memory(){

   // MEM works on the instruction in the IU3_MEM register
    if (IU3_MEM_register.opcode != "") {
        
        string op = IU3_MEM_register.opcode;
        //int address = IU3_MEM_register.result; // The address calculated in EX

        if (op == "LDUR") {

            cout << "dm index is" <<  IU3_MEM_register.result << endl;

            // Read from memory into the result field
            MEM_WB_register.result = data_memory[IU3_MEM_register.result];
            
            cout << "reading " << MEM_WB_register.result << " from data memory" << endl;
        } 
        else if (op == "STUR") {

            // Write the value of the source register into memory
            data_memory[IU3_MEM_register.result] = int_registers[IU3_MEM_register.rd];

        }
      

         

        //copy relevant information
        MEM_WB_register.opcode = IU3_MEM_register.opcode;
        MEM_WB_register.if_exit = IU3_MEM_register.if_exit;
        MEM_WB_register.id_exit = IU3_MEM_register.id_exit;
        MEM_WB_register.ex_exit = IU3_MEM_register.ex_exit;
        MEM_WB_register.mem_exit = cycle;

        MEM_WB_register.rd = IU3_MEM_register.rd;

        MEM_WB_register.number_id = IU3_MEM_register.number_id;

        //dont overwrite saved value
        if (op != "LDUR") {

            MEM_WB_register.result = IU3_MEM_register.result;

        }

        MEM_WB_register.regWrite = IU3_MEM_register.regWrite; 

        cout <<  MEM_WB_register.result << endl;
        cout <<  MEM_WB_register.rd << endl;
        

        instruct_list[MEM_WB_register.number_id].mem_exit = MEM_WB_register.mem_exit;
    }
    else{

    
        MEM_WB_register = Instruction(); 
        MEM_WB_register.opcode = "";

    }
}

void writeBack() {

    // WB works on the instruction in the very last register
    if (MEM_WB_register.opcode != "") {

       printExits(MEM_WB_register);

       // cout << "rd is " << MEM_WB_register.rd << endl;
        
        string op = MEM_WB_register.opcode;

        if (op == "LDUR"){

            // Safety check: Never write to R31 (the zero register)
            if (MEM_WB_register.rd != 31 && MEM_WB_register.rd != -1) {
                cout << "Writing" << endl;

                MEM_WB_register.wb_exit = cycle;
                int_registers[MEM_WB_register.rd] = MEM_WB_register.result;

            }
        }

        //just log information for STUR, as we already moved in data or dont need to move anyhting
        if (op == "STUR" || op == "B" || op == "CBZ" || op == "CBNZ"){

            MEM_WB_register.wb_exit = cycle;

        }

         

        // Instructions that write to a register
        if (op == "ADD" || op == "ADDI" || op == "SUB" || op == "SUBI" || op == "MUL" ||
            op == "AND" || op == "ANDI"|| op == "ORR"  || op == "ORRI" || op == "LSL" || op == "LSR" || 
            op == "LDUR" || op == "MOVZ" || op == "MOVK") {
            
            // Safety check: Never write to R31 (the zero register)
            if (MEM_WB_register.rd != 31 && MEM_WB_register.rd != -1) {
                cout << "Writing" << endl;

                MEM_WB_register.wb_exit = cycle;
                int_registers[MEM_WB_register.rd] = MEM_WB_register.result;
                
            }
        }
        instruct_list[MEM_WB_register.number_id].wb_exit = MEM_WB_register.wb_exit;
      
    }
    else{

    
        MEM_WB_register = Instruction(); 
        MEM_WB_register.opcode = "";

    }
}

//When testing your WB stage, try printing your registers at the very end of each cycle. 
//You will see that even though ADDI shows up in the trace at Cycle 1, the int_registers[rd] 
//value won't change until the end of Cycle 4 or beginning of Cycle 5.


void debugFunction(){

    //debug for fetch
    cout << "fetch_debug" << endl;
    cout << "Cycle " << cycle << " | IF_ID holds: " << IF_ID_register.opcode << endl;
    cout << "Cycle " << cycle << " | ID_IU1 holds: " << ID_IU1_register.opcode << endl;
    cout << "-----------------------------------" << endl;

    //debug for decode
    cout << "decode_debug" << endl;
    cout << "Cycle " << cycle << ":" << endl;
    cout << "  Fetch Stage (IF_ID): " << IF_ID_register.opcode << endl;
    cout << "  Decode Stage (ID_IU1): " << ID_IU1_register.opcode 
    << " | Val1: " << ID_IU1_register.val1 
    << " | Val2: " << ID_IU1_register.val2 << endl;
    cout << "-----------------------------------" << endl;
  

    //debug for result of execution
    cout << "execute_debug" << endl;
    cout << "Cycle " << cycle << ":" << endl;
    cout << "  Execute Stage (IU1_IU2): " << IU3_MEM_register.opcode 
    << " | Result: " << IU3_MEM_register.result << endl;
    cout << "-----------------------------------" << endl;


    //debug writeBack
    cout << "writeBack_debug" << endl;
    cout << "Destination Register Location: " << MEM_WB_register.rd << endl;
    cout << "value: " << int_registers[MEM_WB_register.rd] << endl;



    //debug time info
    cout << "time info_debug" << endl;
    cout <<  "if_exit: " << MEM_WB_register.if_exit << endl;
    cout << "id_exit: " << MEM_WB_register.id_exit << endl;
    cout <<  "ex_exit: " << MEM_WB_register.ex_exit << endl;
    cout <<  "mem_exit: " << MEM_WB_register.mem_exit << endl;
    cout <<  "wb_exit: " << MEM_WB_register.wb_exit << endl;
    cout << "-----------------------------------" << endl;
    cout << "-----------------------------------" << endl;
    cout << endl;
    cout << endl;




        
    

        /*
        cout << "--- Cycle " << cycle << " ---" << endl;
        cout << "ID_IU1_register Opcode: " << ID_IU1_register.opcode << endl;
        cout << "ID_IU1_register Val1 (Rn): " << ID_IU1_register.val1 << endl;
        cout << "ID_IU1_register Val2 (Rm/Imm): " << ID_IU1_register.val2 << endl;
        cout << "----------------------" << endl;
        */

        /*
        if (IU1_IU2_register.opcode != "EMPTY") {
            cout << "Execute Stage | Op: " << IU1_IU2_register.opcode 
            << " | " << IU1_IU2_register.val1 << " " << IU1_IU2_register.opcode << " " << IU1_IU2_register.val2 
            << " = " << IU1_IU2_register.result << endl;
        }
        */

        /*
        // Inside your while loop, after all functions and movements:
        cout << "CYCLE " << cycle << " DEBUG:" << endl;
        cout << "  1. IF_ID   Op: " << IF_ID_register.opcode << endl;
        cout << "  2. ID_IU1  Op: " << ID_IU1_register.opcode << " | V1: " << ID_IU1_register.val1 << " | V2: " << ID_IU1_register.val2 << endl;
        cout << "  3. IU1_IU2 Op: " << IU1_IU2_register.opcode << " | Res: " << IU1_IU2_register.result << endl;
        */

        /*
        // --- FINAL PIPELINE SNAPSHOT ---
        cout << "================ CYCLE " << cycle << " ================" << endl;
        cout << " [IF]  register: " << IF_ID_register.opcode << endl;

        cout << " [ID]  register: " << ID_IU1_register.opcode 
            << " | V1: " << ID_IU1_register.val1 
            << " | V2: " << ID_IU1_register.val2 << endl;

        cout << " [EX]  register: " << IU1_IU2_register.opcode 
            << " | Result: " << IU1_IU2_register.result << endl;

        cout << " [MEM] register: " << IU3_MEM_register.opcode 
            << " | Result: " << IU3_MEM_register.result << endl;

        cout << " [WB]  register: " << MEM_WB_register.opcode 
            << " | Writing " << MEM_WB_register.result << " to R" << MEM_WB_register.rd << endl;
        cout << "==========================================" << endl << endl;
        */


}


void printOutputFile(string filename) {
    
    ofstream outFile(filename);

    // Header
    outFile << left << setw(10) << "Label" << setw(20) << "Instruction" 
            << setw(6) << "IF" << setw(6) << "ID" << setw(10) << "EX" 
            << setw(6) << "MEM" << setw(6) << "WB" << endl;

  // 2. The Loop
    for (const auto& inst : instruct_list) {
        

        // Print the original string you saved during parsing
        outFile << left << setw(30) << inst.raw_line;

        // Helper lambda to print a number or a blank space if it's -1
        auto printStage = [&](int cycle, int width) {
            if (cycle != -1){
                 outFile << right << setw(width) << cycle;
            } 
           
            else{
                outFile << setw(width) << " ";
            } 
        };

        // Print each column
        printStage(inst.if_exit, 2);
        printStage(inst.id_exit, 6);
        printStage(inst.ex_exit, 5);
        printStage(inst.mem_exit, 10);
        printStage(inst.wb_exit, 6);

        outFile << endl;
    }

    // 3. Print Cache Stats (using your global counter variables)
    outFile << "\nTotal number of access requests for instruction cache: " << i_req << endl;
    outFile << "Number of instruction cache hits: " << i_hit << endl;
    outFile << "\nTotal number of access requests for data cache: " << d_req << endl;
    outFile << "Number of data cache hits: " << d_hit << endl;

    outFile.close();


}
  

void passTime(Instruction newIntruct, Instruction oldIntruct){


}

void printExits(Instruction instruct){

    cout << "Running printExits for " << instruct.opcode << endl;

    cout << "if_exit: " << instruct.if_exit << endl;
    cout << "id_exit: " << instruct.id_exit << endl;
    cout << "ex_exit: " << instruct.ex_exit << endl;
    cout << "mem_exit: " << instruct.mem_exit << endl;
    cout << "wb_exit: " << instruct.wb_exit << endl;

}



void executeHazardCheck(){

    string op = ID_IU1_register.opcode;

    cout << "EX is looking for R" << ID_IU1_register.rn << endl;
    cout << "IU3_MEM contains R" << IU3_MEM_register.rd << endl;
    cout << "MEM_WB contains R" << MEM_WB_register.rd << endl;

    //R[Rd] = R[Rn] + R[Rm]
    if(op == "ADD" || op == "MUL" || op == "SUB" || op == "AND" || op == "ORR"){
        
        //Check Rn (Source 1) 

        //EX/MEM hazard
        if (IU3_MEM_register.regWrite && IU3_MEM_register.rd != 31 && IU3_MEM_register.rd == ID_IU1_register.rn) {

            cout << " c1" << endl;
            cout << endl;

            ID_IU1_register.val1 = IU3_MEM_register.result;

        }

        //MEM/WB hazard
        else if (MEM_WB_register.regWrite && MEM_WB_register.rd != 31 && MEM_WB_register.rd == ID_IU1_register.rn) {

              cout << " c2" << endl;
              cout << endl;

            ID_IU1_register.val1 = MEM_WB_register.result;
        }
        
        //Check Rm (Source 2).
        //EX/MEM hazard
        if (IU3_MEM_register.regWrite && IU3_MEM_register.rd != 31 && IU3_MEM_register.rd == ID_IU1_register.rm) {

            ID_IU1_register.val2 = IU3_MEM_register.result;

        }

        //MEM/WB hazard
        else if (MEM_WB_register.regWrite && MEM_WB_register.rd != 31 && MEM_WB_register.rd == ID_IU1_register.rm) {

            ID_IU1_register.val2 = MEM_WB_register.result;
        }
    }

    if(op == "ADDI" || op == "SUBI" || op == "ANDI" ||  op == "ORRI" || op == "LSL" || op == "LSR" || op == "LDUR"){

        //Check only Rn. (The second operand is an immediate, so it can't have a data hazard).
        //EX/MEM hazard
        if (IU3_MEM_register.regWrite && IU3_MEM_register.rd != 31 && IU3_MEM_register.rd == ID_IU1_register.rn) {

            cout << "cond1" << endl;

            ID_IU1_register.val1 = IU3_MEM_register.result;

        }

        //MEM/WB hazard
        else if (MEM_WB_register.regWrite && MEM_WB_register.rd != 31 && MEM_WB_register.rd == ID_IU1_register.rn) {

            cout << "cond2" << endl;

            ID_IU1_register.val1 = MEM_WB_register.result;
        }

    }

    if (op == "STUR"){

        /*

        //Check Rn (Base address) 
        if (EX_MEM_register.regWrite && EX_MEM_register.rd != 31 && EX_MEM_register.rd == ID_IU1_register.rn) {
            final_Rn = EX_MEM_register.alu_result;
        }
        else if (IU3_MEM_register.regWrite && IU3_MEM_register.rd != 31 && IU3_MEM_register.rd == ID_IU1_register.rn) {
            // If the previous instruction was a Load, grab its memory data, otherwise its ALU result
            final_Rn = (IU3_MEM_register.opcode == "LDUR") ? IU3_MEM_register.mem_data : IU3_MEM_register.alu_result;
        }
        else {
            final_Rn = ID_IU1_register.val_rn;
        }
        
        
        //Check Rt (The data being stored is a source here!).
        if (EX_MEM_register.regWrite && EX_MEM_register.rd != 31 && EX_MEM_register.rd == ID_IU1_register.rt) {
        final_Rt = EX_MEM_register.alu_result;
        }
        else if (IU3_MEM_register.regWrite && IU3_MEM_register.rd != 31 && IU3_MEM_register.rd == ID_IU1_register.rt) {
            final_Rt = (IU3_MEM_register.opcode == "LDUR") ? IU3_MEM_register.mem_data : IU3_MEM_register.alu_result;
        }

        */


    }


    if (op == "CBZ" || op == "CBNZ"){

        //Check Rt. (This is the register being compared to zero).

    } 

    if (op == "MOVK"){

        //Check Rd. Since MOVK keeps the other bits of the register, the current value of Rd is actually a source!

    }
}