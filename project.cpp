#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <map>
#include <iomanip>
using namespace std;


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
    FP_TYPE, //Floating Point (FADD, FSUB, FMUL)
    
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
const vector<string> fp_type = {"FADD", "FSUB", "FMUL"};

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


    int latency = 1;         

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

    //store the corresponding line form instr.txt for output file
    string raw_line= "";

    int number_id = -1;


    long long result_data = 0;


    //additional helper flags
    bool is_fp = false; 

    bool regWrite = false;
};


//keep track if intrucion caceh miss cycles, hits, and requests
int icache_miss_delay = 0;
int i_req = 0, i_hit = 0;

//stores intruction cache
struct ICacheLine {
    bool valid = false;
    int tag = -1;
};


ICacheLine i_cache[4]; 

// Help the simulator know how to split the address
struct ICacheParts { int tag; int index; };
ICacheParts splitICache(int address) {
    ICacheParts p;
    p.index = (address >> 4) & 0x3; // Bits 4-5 for index
    p.tag = (address >> 6);        // Bits 6+ for tag
    return p;
}








// The Shared Bus clock for the both caches
int memory_bus_busy_until = 0; 

struct DCacheLine {
    bool valid = false;
    bool dirty = false;
    int tag = -1;
    int lru_counter = 0; // For LRU replacement
};

// 2 sets, 2 ways per set = 4 blocks total
DCacheLine d_cache[2][2]; 


int dcache_stall_cycles = 0;

int d_req = 0;
int d_hit = 0;



// Vector to hold the instructions read from the file
vector<Instruction> instruct_list; 

//store labels with line number for branch instructions
map<string, int> labels = {};



//Pipeline registers
Instruction IF_ID_register;

Instruction ID_EX_register;
Instruction ID_FLOAT_EX_register;

Instruction EX_MEM_register;

Instruction MEM_WB_register;

Instruction post_WB_register_info;


//transitions an intruction to IF_ID register
Instruction fetched_instruction;

bool instruciton_fetched = false;





//keep track of the cycle and current instruction being executed 
int PC = 0; 

int cycle = 0;

//enabled if a halt instruction is read in decode, will stop the while loop and end the program
bool activeHalt = false;



vector<string> line_list; // Vector to hold string of instructions read from the file



//load data from the datafile
void loadData(string filename);

//for the parser to create the instruction struct from the text file
struct Instruction createInstruction(vector<string>& tokens);


//load instructions from the instruction file, parse them, and store in vector
void readInstructions(string filename);

//organize opcodes into types 
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


//return the results of the program to the output file
void printOutputFile(string filename);

//check for data / structrual hazards
void executeHazardCheck();

//move each of the pipeline registers foward each cycle
void moveRegisters();


//testing functions
void testData();

void debugFunction();

void printInstructions();

void printExits(Instruction instruct);



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




int main(int argc, char* argv[]){

    // Check if the user provided 3 arguments (plus the program name itself)
    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " inst.txt data.txt output.txt" << endl;
        return 1;
    }

    // Capture the filenames from the command line
    string instFile = argv[1];
    string dataFile = argv[2];
    string outFile  = argv[3];

    //load from memory
    loadData(dataFile);

    //load instruction files
    readInstructions(instFile);
  

    //Add raw line and number id to each instruction for later use in output file
    for (std::size_t i = 0; i < instruct_list.size(); ++i) {
        instruct_list[i].raw_line =  line_list[i];
        instruct_list[i].number_id = i;
    }


    //create a loop to run the simulator until we hit a HALT instruction or run out of instructions
    //the loop runs backwards to have the stages increment foward once each time in the while loop
    while (true) {

        cycle++;

        //check if there are currently hazards in pipeline
        executeHazardCheck();


        // 3. Writeback (WB)
        writeBack();

        // 3. Memory (MEM)
        memory();
        
        
        // 2. Execute (EX)
        execute();
        

  
        // 1. Decode (ID)
        decode();    
                
        
        fetch(instruct_list);

        //move pipeline registers foward after the loop
        moveRegisters();



        //decrease stall count of cache misses
        if (dcache_stall_cycles > 0) dcache_stall_cycles--;
        if (icache_miss_delay > 0) icache_miss_delay--;
   
        //decrease stall count of a instruciton that takes long in execute by 
        if(ID_EX_register.latency > 0) ID_EX_register.latency--;
        if(ID_FLOAT_EX_register.latency > 0) ID_FLOAT_EX_register.latency--;

        //The cycle will break when halt is read in decode, 
        //the other 999 case is a failsafe to prevent infinte loops
        if (activeHalt || cycle > 999) break; 
    }

    //prin results to output file
    printOutputFile(outFile);
    
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

            }

            else{           
                //line is each indivdual word
                tokens.push_back(line);

            }
        }

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

    // safety check, return immedately if empty
    if (tokens.empty()) return instr;



    //organize intruction data based on if its each type
    string opcode = tokens[0];

    instr.opcode = opcode;

    

    // R_type Arithmetic (ADD, SUB, MUL, AND, ORR), R[Rd] = R[Rn] + R[Rm]
    if (find(r_type.begin(), r_type.end(), opcode) != r_type.end()) {


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

        instr.rd = stoi(tokens[1].substr(1)); // Rd
        instr.rn = stoi(tokens[2].substr(1)); // Rn
        instr.immediate = stoll(tokens[3].substr(1)); // Immediate value

        instr.regWrite = true;

        instr.type = InstType::SHIFT_TYPE;
    }

    //FP TYPE, Floating Point (FADD, FSUB, FMUL) R[Rd] = R[Rn] + R[Rm]
    else if (checkOpcode(opcode, fp_type)) {

        instr.rd = stoi(tokens[1].substr(1)); // Rd
        instr.rn = stoi(tokens[2].substr(1)); // Rn
        instr.rm = stoi(tokens[3].substr(1)); // Rm


        instr.type = InstType::FP_TYPE;

        if (opcode == "FADD" || opcode == "FSUB"){
            instr.latency = 3; // 3 cycles for FADD and FSUB
        }

        else if (opcode == "FMUL"){
            instr.latency = 6; // 6 cycles for FMUL
        }

        instr.regWrite = true;
        instr.is_fp = true;
    } 


    //I_TYPE, Immediate (ADDI, SUBI, ANDI, ORRI) R[Rd] = R[Rn] + Immediate    
    else if (checkOpcode(opcode, i_type)) {

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

        instr.rd = stoi(tokens[1].substr(1)); // Rd
        instr.immediate = stoll(tokens[2].substr(1)); // Immediate value

        //in teh case the LSL is listed
        if (tokens.size() >= 5) {

            instr.shift_amount = stoll(tokens[4]); // Shift amount
        
        }

        else {
            instr.shift_amount = 0; // Default shift amount is 0 if not specified
        }

        instr.type = InstType::IM_TYPE;

        instr.regWrite = true;
        
    } 

    //Data transfer (STUR, LDUR, LDURD, STURD) 
    else if (checkOpcode(opcode, d_type)) {


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

        instr.rn = stoi(tokens[1].substr(1)); // Rn
        instr.label = tokens[2]; // Label for branch target

        instr.type = InstType:: CB_TYPE;
    } 

    //B_TYPE, //Unconditional Branch (B)
    else if (opcode == B_TYPE) {

        instr.label = tokens[1]; // Label for branch target

        instr.type = InstType:: B_TYPE;
    }
    //HALT_TYPE, //HALT (HLT)
    else if (opcode == HALT_TYPE) {

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
void fetch(vector<Instruction>& program){

    //Dont move in the case of a fetched intruciton not being transfered yet, or a miss that needs to wait
    if (instruciton_fetched == true || icache_miss_delay > 0 || activeHalt == true) {
        
        return; 
    }

    // If the PC is within the bounds of instruction vector
    if ( PC / 4 < int(program.size()) )  {
        ICacheParts p = splitICache(PC);
        i_req++;


        if (i_cache[p.index].valid && i_cache[p.index].tag == p.tag) {

            // cache hit:  move the PC 
            i_hit++;

         
            //start to load IF ID Loaded on success
            fetched_instruction = program[PC / 4];
            instruciton_fetched = true;

            // Move to the next instruction address
            PC += 4; 

        } 
    
        else {
            // in the case of a miss check if the bus is free, claim if so
            if (cycle >= memory_bus_busy_until) {

                //start 12 cycle wait while ensuring the PC isn't incremented.
                icache_miss_delay = 12;
                i_cache[p.index].valid = true;
                i_cache[p.index].tag = p.tag;
            }
        }
    }
}


//look up values in register file, put them in the ID_EX_register for the next stage
void decode(){

    // grab the register values If it's a real instruction or not EMPTY, 
    if (IF_ID_register.opcode != "") {


        string op = IF_ID_register.opcode;

        // Read the first source register value

        // XZR should always return 0
        if (IF_ID_register.rn == 31) {
            IF_ID_register.val1 = 0;
        }

        else if(IF_ID_register.opcode == "MOVK"){
             IF_ID_register.val1 = int_registers[IF_ID_register.rd];

        }

        else{
            //if the instruction uses rn, or first source register look up its value in ophysical register array      
           IF_ID_register.val1 = int_registers[IF_ID_register.rn];
        }

         //Second source register or immediate value

         // For ADDI or SUBI, use immediate is already in struct.
        if (op == "ADDI" || op == "SUBI" || op == "LDUR" || op == "STUR" ||
            op == "LSL" || op == "LSR" || op == "ANDI" || op == "ORRI" ||
           op == "MOVZ" || op == "MOVK") {
                
            IF_ID_register.val2 = IF_ID_register.immediate;
        }

        else if (IF_ID_register.rn == 31) {
            IF_ID_register.val2 = 0;
        }

        else{
            IF_ID_register.val2 = int_registers[IF_ID_register.rm];
        }

        //get stur value now for later use
        if (op == "STUR") {
            IF_ID_register.result_data = int_registers[IF_ID_register.rd];
        }
    }
}


void execute(){

    //return if the latency has ran out yet
    if (ID_EX_register.latency > 0) return;

    if (ID_EX_register.opcode != "") {
    
        string op = ID_EX_register.opcode;
        

        // level 3 (IU3) 
        if (op == "MUL") {

            //perform mul operation
            ID_EX_register.result = ID_EX_register.val1 * ID_EX_register.val2;
        } 

        // level 2 (IU2)
        else if (op == "ADD" || op == "ADDI" || op == "SUB" || op == "SUBI"){


            // ADD and SUB finish their 2nd cycle here
            if (op == "ADD" || op == "ADDI") {
                
                ID_EX_register.result = ID_EX_register.val1 + ID_EX_register.val2;
        
                } 
            if (op == "SUB" || op == "SUBI") {

                ID_EX_register.result = ID_EX_register.val1 - ID_EX_register.val2;
            }

            return;
        }


        //level 1 (IU1)
        else if (op == "AND" || op == "ANDI" || op == "ORR" || op == "ORRI" || op == "LSL" || op == "LSR"){

            if (op == "AND" || op == "ANDI") {

                ID_EX_register.result = ID_EX_register.val1 & ID_EX_register.val2;
            }

            else if (op == "ORR" || op == "ORRI") {

                ID_EX_register.result = ID_EX_register.val1 | ID_EX_register.val2;
            }

            // shifts (LSL/LSR)
            else if (op == "LSL") {

                ID_EX_register.result = (uint32_t)ID_EX_register.val1 << ID_EX_register.val2;
            }

            else if (op == "LSR") {

                ID_EX_register.result = (uint32_t)ID_EX_register.val1 >> ID_EX_register.val2;
            }
        }

        //loading and storing
        if (op == "LDUR" || op == "LDURD" || op == "STUR" || op == "STURD"){

            if (ID_EX_register.latency > 0) {
                return; 
            }

            //calculate immedate address
            ID_EX_register.result = ID_EX_register.val1 + ID_EX_register.val2;
        }

        if( op == "MOVZ"){

            //shift
            ID_EX_register.result = ID_EX_register.val2 << ID_EX_register.shift_amount;
        }

        if(op == "MOVK"){

            uint32_t shift = ID_EX_register.shift_amount * 16;
            uint32_t immediate = ID_EX_register.val2;
            uint32_t currentVal1 = ID_EX_register.val1;


            if (shift < 32) {

                uint32_t mask = ~(0xFFFF << shift);

                ID_EX_register.result = (currentVal1 & mask) | (immediate << shift);
            } else {

                // if shift is 32 or more, we keep the whole thing as shift is out of bounds
                ID_EX_register.result = ID_EX_register.val1;
            }
        }

        if(op == "B"){

            // look up the line number in map
            if (labels.find(ID_EX_register.label) != labels.end()) {
                int targetLine = labels[ID_EX_register.label];


                //Update the PC to that line
                PC = targetLine*4 - 4;


                //Flush past intrucitons

                //Initial fetched instruction
                fetched_instruction = Instruction(); 
                fetched_instruction.opcode = "";

            
                //Decode
                IF_ID_register = Instruction(); 
                IF_ID_register.opcode = "";

                return;
                
            } 
            else {
                cerr << "Error: Label '" << ID_EX_register.label << "' not found!" << endl;
            }
        }
        if(op == "CBZ" || op == "CBNZ"){

            // look up line number in map
            if (labels.find(ID_EX_register.label) != labels.end()) {
                int targetLine = labels[ID_EX_register.label];

                //CBZ jump if zero
                //CBNZ jump if not zero
                if( (op == "CBZ" && ID_EX_register.val1 == 0) || (op == "CBNZ" && ID_EX_register.val1 != 0)){

                    //Update the PC to that line
                    PC = targetLine*4 - 4;

                    //Flush past intrucitons

                    //Initial fetched instruction
                    fetched_instruction = Instruction(); 
                    fetched_instruction.opcode = "";

                
                    //Decode
                    IF_ID_register = Instruction(); 
                    IF_ID_register.opcode = "";

                    return;
                }
            } 
            else {
                cerr << "Error: Label '" << ID_EX_register.label << "' not found!" << endl;
            }

        }
    }
}

void memory(){


    //if stalling if cycles havent ran out yet
    if (dcache_stall_cycles > 0) {

        return;    
    }

   

    //pass throught the function, if not LDUR or stur
    string op = EX_MEM_register.opcode;

    if (op != "LDUR" && op != "LDURD" && op != "STUR" && op != "STURD") {
      
    
        return;
    }



   // MEM works on the instruction in the EX_MEM register
    if (EX_MEM_register.opcode != "") {
        
        string op = EX_MEM_register.opcode;
        //int address = EX_MEM_register.result; // The address calculated in EX


        int addr = EX_MEM_register.result;
        int set = (addr >> 4) & 0x1; // 2 sets, so use 1 bit
        int tag = (addr >> 5);       // Remaining bits are tag


        // 1. Search both ways
        int hit_way = -1;

        for(int i = 0; i < 2; i++) {

            if(d_cache[set][i].valid && d_cache[set][i].tag == tag) {

                hit_way = i;

                break;
            }
        }

        if (hit_way != -1) {

            // cache hit: perform the load/store and update LRU
            d_req++; 
            d_hit++;
            d_cache[set][hit_way].lru_counter = cycle; // Update LRU

                if (op == "LDUR") {
                    //MEM_WB_register.mem_data = data_memory[addr / 4];

                    int adjusted_index = (addr - 256) / 4; // 256 becomes 0, 260 becomes 1, etc.
                    EX_MEM_register.result = data_memory[adjusted_index];
                } 
                
                else if (op == "STUR") {
                    // WRITE-BACK: Update cache only, mark dirty
                    d_cache[set][hit_way].dirty = true;

                    data_memory[addr / 4] = EX_MEM_register.result_data;
                }

            return;

        }

        else {
            // --- CACHE MISS ---
            // I-Cache has priority. We only proceed if Fetch isn't missing 
            // AND the bus is free.
            if (cycle >= memory_bus_busy_until) {
                d_req++;
                
                // Determine LRU way to replace
                int way = (d_cache[set][0].lru_counter <= d_cache[set][1].lru_counter) ? 0 : 1;
                
                int penalty = 12; // 12 cycles to read the block
                
                // If the block being kicked out is dirt, write it back (+12 cycles)
                if (d_cache[set][way].valid && d_cache[set][way].dirty) {
                    penalty += 12;
                }

                memory_bus_busy_until = cycle + penalty;
                dcache_stall_cycles = penalty - 1;

                // replace and update info
                d_cache[set][way].valid = true;
                d_cache[set][way].tag = tag;
                d_cache[set][way].dirty = (op == "STUR");
                d_cache[set][way].lru_counter = cycle;

        
                return; // Stay in EX_MEM_register until the stall is over
            }
  
    }

    }
}

void writeBack() {

    // WB works on the instruction in the very last register
    if (MEM_WB_register.opcode != "") {
        
        string op = MEM_WB_register.opcode;

        if (op == "LDUR" || op == "LDURD"){

            //never write to R31 (the zero register)
            if (MEM_WB_register.rd != 31 && MEM_WB_register.rd != -1) {

                MEM_WB_register.wb_exit = cycle;
                int_registers[MEM_WB_register.rd] = MEM_WB_register.result;

            }
        }

        //just log information for STUR, as we already moved in data or dont need to move anyhting
        //Float operaitions as well
        if (op == "STUR" || op == "STURD" || op == "B" || op == "CBZ" || op == "CBNZ" ||
            op == "FADD" || op == "FSUB" || op == "FMUL" || op == "LDURD"){

            MEM_WB_register.wb_exit = cycle;

        }

         

        // Instructions that write to a register
        if (op == "ADD" || op == "ADDI" || op == "SUB" || op == "SUBI" || op == "MUL" ||
            op == "AND" || op == "ANDI"|| op == "ORR"  || op == "ORRI" || op == "LSL" || op == "LSR" || 
            op == "LDUR" ||  op == "MOVZ" || op == "MOVK") {
            
            // never write to R31 (the zero register)
            if (MEM_WB_register.rd != 31 && MEM_WB_register.rd != -1) {

                MEM_WB_register.wb_exit = cycle;
                int_registers[MEM_WB_register.rd] = MEM_WB_register.result;
                
            }
        }

        instruct_list[MEM_WB_register.number_id].wb_exit = MEM_WB_register.wb_exit;


        //clear out register after use
        MEM_WB_register = Instruction(); 
        MEM_WB_register.opcode = "";
    }
}


void debugFunction(){

    //debug for fetch
    cout << "fetch_debug" << endl;
    cout << "Cycle " << cycle << " | IF_ID holds: " << IF_ID_register.opcode << endl;
    cout << "Cycle " << cycle << " | ID_EX holds: " << ID_EX_register.opcode << endl;
    cout << "-----------------------------------" << endl;

    //debug for decode
    cout << "decode_debug" << endl;
    cout << "Cycle " << cycle << ":" << endl;
    cout << "  Fetch Stage (fetched instruc): " << fetched_instruction.opcode << endl;
    cout << "  Decode Stage (ID_EX): " << IF_ID_register.opcode 
    << " | Val1: " << IF_ID_register.val1 
    << " | Val2: " << IF_ID_register.val2 << endl;
    cout << "-----------------------------------" << endl;
  

    //debug for result of execution
    cout << "execute_debug" << endl;
    cout << "Cycle " << cycle << ":" << endl;
    cout << "  Execute Stage (IU1_IU2): " << ID_EX_register.opcode 
    << " | Result: " << ID_EX_register.result << endl;
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



}


void printOutputFile(string filename) {
    
    ofstream outFile(filename);

    // Make the header
    outFile << left << setw(10) << "Cycle Number for each stage   "
            << setw(6) << "IF" << setw(5) << "ID" << setw(9) << "EX" 
            << setw(5) << "MEM" << setw(7) << "  WB" << endl;

  // loop intrucitons and exits
    for (const auto& inst : instruct_list) {
        

        // print the original string you saved during parsing
        outFile << left << setw(30) << inst.raw_line;

        // helper lambda to print a number or a blank space if it's -1
        auto printStage = [&](int cycle, int width) {
            if (cycle != -1){
                 outFile << right << setw(width) << cycle;
            } 
           
            else{
                outFile << setw(width) << " ";
            } 
        };

        // print each column
        printStage(inst.if_exit, 2);
        printStage(inst.id_exit, 6);
        printStage(inst.ex_exit, 5);
        printStage(inst.mem_exit, 10);
        printStage(inst.wb_exit, 6);

        outFile << endl;
    }

    // print cache stats 
    outFile << "\nTotal number of access requests for instruction cache: " << i_req << endl;
    outFile << "Number of instruction cache hits: " << i_hit << endl;
    outFile << "\nTotal number of access requests for data cache: " << d_req << endl;
    outFile << "Number of data cache hits: " << d_hit << endl;

    outFile.close();
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

    string op = ID_EX_register.opcode;

    string float_op = ID_FLOAT_EX_register.opcode;

    

    // float data hazard
    // if an instruction in the FP pipeline needs a register currently being loaded or calculated
    if (ID_FLOAT_EX_register.opcode != "") {
        bool hazard = false;
        
        // check if source 1 or source 2 is being written to by instructions ahead in the pipeline
        if (EX_MEM_register.regWrite && (ID_FLOAT_EX_register.rn == EX_MEM_register.rd || ID_FLOAT_EX_register.rm == EX_MEM_register.rd)) {
            hazard = true;
        }
        if (MEM_WB_register.regWrite && (ID_FLOAT_EX_register.rn == MEM_WB_register.rd || ID_FLOAT_EX_register.rm == MEM_WB_register.rd)) {
            hazard = true;
        }

        if (hazard) {
            //setting a latency stall
            ID_FLOAT_EX_register.latency++; 
        }
    }
    






        //check load use hazards
        if (EX_MEM_register.opcode == "LDUR" || EX_MEM_register.opcode == "LDURD") {

            bool needsLoadData = false;

            // check if instruction uses the register LDUR is currently fetching
            if (EX_MEM_register.rd != 31) {

                if (EX_MEM_register.rd == ID_EX_register.rn || EX_MEM_register.rd == ID_EX_register.rm) {
                    needsLoadData = true;
                }
                // special case for STUR: rd is also a source register
                if ((ID_EX_register.opcode == "STUR" || ID_EX_register.opcode == "STUR")  && EX_MEM_register.rd == ID_EX_register.rd) {
                    needsLoadData = true;
                }
            }

            if (needsLoadData) {
                ID_EX_register.latency = 2; // bubble to let LDUR reach MEM_WB
                return; 
            }
        }

        //R[Rd] = R[Rn] + R[Rm]
        if(op == "ADD" || op == "MUL" || op == "SUB" || op == "AND" || op == "ORR"){
            
            //Check rn or source 1

            //EX/MEM hazard
            if (EX_MEM_register.regWrite && EX_MEM_register.rd != 31 && EX_MEM_register.rd == ID_EX_register.rn) {

                ID_EX_register.val1 = EX_MEM_register.result;

            }

            //MEM/WB hazard
            else if (MEM_WB_register.regWrite && MEM_WB_register.rd != 31 && MEM_WB_register.rd == ID_EX_register.rn) {

                ID_EX_register.val1 = MEM_WB_register.result;
            }
            
            //Check rm or source 2

            //EX/MEM hazard
            if (EX_MEM_register.regWrite && EX_MEM_register.rd != 31 && EX_MEM_register.rd == ID_EX_register.rm) {

                ID_EX_register.val2 = EX_MEM_register.result;

            }

            //MEM/WB hazard
            else if (MEM_WB_register.regWrite && MEM_WB_register.rd != 31 && MEM_WB_register.rd == ID_EX_register.rm) {

                ID_EX_register.val2 = MEM_WB_register.result;
            }
        }

        if(op == "ADDI" || op == "SUBI" || op == "ANDI" ||  op == "ORRI" || op == "LSL" || op == "LSR" || op == "LDUR" || op == "LDURD"){

            //check only rn. (second operand is an immediate, so it can't have a data hazard)
            //EX/MEM hazard
            if (EX_MEM_register.regWrite && EX_MEM_register.rd != 31 && EX_MEM_register.rd == ID_EX_register.rn) {

                ID_EX_register.val1 = EX_MEM_register.result;

            }

            //MEM/WB hazard
            else if (MEM_WB_register.regWrite && MEM_WB_register.rd != 31 && MEM_WB_register.rd == ID_EX_register.rn) {

                ID_EX_register.val1 = MEM_WB_register.result;
            }

        }

        if (op == "STUR" || op == "STURD"){


            //check rn (Base address) 
            if (EX_MEM_register.regWrite && EX_MEM_register.rd != 31 && EX_MEM_register.rd == ID_EX_register.rn) {

                ID_EX_register.val1 = EX_MEM_register.result;

            }

            else if (MEM_WB_register.regWrite && MEM_WB_register.rd != 31 && MEM_WB_register.rd == ID_EX_register.rn) {

                ID_EX_register.val1 = MEM_WB_register.result;

            }
            
            
            //check rd, source data
            if (EX_MEM_register.regWrite && EX_MEM_register.rd != 31 && EX_MEM_register.rd == ID_EX_register.rd) {

                    ID_EX_register.result_data = EX_MEM_register.result; 

            }
            else if (MEM_WB_register.regWrite && MEM_WB_register.rd != 31 && MEM_WB_register.rd == ID_EX_register.rd) {

                    ID_EX_register.result_data = MEM_WB_register.result;
            }
        }


        if (op == "CBZ" || op == "CBNZ"){

        
            if (EX_MEM_register.regWrite && EX_MEM_register.rd != 31 && EX_MEM_register.rd == ID_EX_register.rn) {

                ID_EX_register.val1 = EX_MEM_register.result;
            }

            // forward from MEM_WB stage
            else if (MEM_WB_register.regWrite && MEM_WB_register.rd != 31 && MEM_WB_register.rd == ID_EX_register.rn) {

                ID_EX_register.val1 = MEM_WB_register.result;
            }

        } 

        if (op == "MOVK"){

            //check rd. 
            if (EX_MEM_register.regWrite && EX_MEM_register.rd != 31 && EX_MEM_register.rd == ID_EX_register.rd) {

                ID_EX_register.val1 = EX_MEM_register.result;

            }
            // forward from MEM_WB stage
            else if (MEM_WB_register.regWrite && MEM_WB_register.rd != 31 && MEM_WB_register.rd == ID_EX_register.rd) {

                ID_EX_register.val1 = MEM_WB_register.result;

            }

        }
    
}

//pipiline registers can move if not occupied
void moveRegisters(){
   

    //only mpove foward if next pipeline register is empty & current isnt stalling
    if (MEM_WB_register.opcode == "" 
        && dcache_stall_cycles == 0  
        && (EX_MEM_register.opcode != "")){


        MEM_WB_register = EX_MEM_register;



        //update leaving value
        MEM_WB_register.mem_exit = cycle;
        instruct_list[MEM_WB_register.number_id].mem_exit = MEM_WB_register.mem_exit;

        //if you move the Register you can empty it out
        EX_MEM_register = Instruction(); 
        EX_MEM_register.opcode = "";

    }

    //float (ID_FLOAT_EX to EX_MEM)
    //only move is next pipeline register is empty and is's finished in the IU
    //Moves first oposed to regualr, so it runs first
    if(EX_MEM_register.opcode == "" 
        && ID_FLOAT_EX_register.latency == 0
        && dcache_stall_cycles == 0
        && (ID_FLOAT_EX_register.opcode != "")){


        EX_MEM_register = ID_FLOAT_EX_register;

        

        //update leaving value
        EX_MEM_register.ex_exit = cycle;
        instruct_list[EX_MEM_register.number_id].ex_exit = EX_MEM_register.ex_exit;

        //if you move the Register you can empty it out
        ID_FLOAT_EX_register = Instruction(); 
        ID_FLOAT_EX_register.opcode = "";
    }


    //regular (ID_EX to EX_MEM)
    //only move is next pipeline register is empty and is's finished in the IU
    else if(EX_MEM_register.opcode == "" 
        && ID_EX_register.latency == 0
        && dcache_stall_cycles == 0
        && (ID_EX_register.opcode != "")){


        EX_MEM_register = ID_EX_register;


        

        //update leaving value
        EX_MEM_register.ex_exit = cycle;
        instruct_list[EX_MEM_register.number_id].ex_exit = EX_MEM_register.ex_exit;

        //if you move the Register you can empty it out
        ID_EX_register = Instruction(); 
        ID_EX_register.opcode = "";
    }


    //Float (IF_ID to ID_EX)
    if(ID_FLOAT_EX_register.opcode == ""
    && (IF_ID_register.opcode != "")
    && (IF_ID_register.is_fp == true)){

      
        ID_FLOAT_EX_register = IF_ID_register;
  

        //update leaving value
        ID_FLOAT_EX_register.id_exit = cycle;
        instruct_list[ID_FLOAT_EX_register.number_id].id_exit = ID_FLOAT_EX_register.id_exit;
        

        //if you move the IF/ID Register you can empty it out
        IF_ID_register = Instruction(); 
        IF_ID_register.opcode = "";

    }




    //regular (IF_ID to ID_EX)
    //only move if next pipeline register is free and current isnt stalling
    else if(ID_EX_register.opcode == ""
    && (IF_ID_register.opcode != "")
    && (IF_ID_register.is_fp == false)){

      
        ID_EX_register = IF_ID_register;
        
         

        //update leaving value
        ID_EX_register.id_exit = cycle;
        instruct_list[ID_EX_register.number_id].id_exit = ID_EX_register.id_exit;



        
        //activate halt if it's a halt instruction
        if (ID_EX_register.opcode == "HLT") {

            activeHalt = true; 
         
        }
        

        //if you move the IF/ID Register you can empty it out
        IF_ID_register = Instruction(); 
        IF_ID_register.opcode = "";

    }



     //start pipeline if we have an instrucion
    if(IF_ID_register.opcode == "" 
        && icache_miss_delay == 0 
        && instruciton_fetched == true){



        IF_ID_register = fetched_instruction;
        instruciton_fetched = false;
        

        //update leaving value
        IF_ID_register.if_exit = cycle;
        instruct_list[IF_ID_register.number_id].if_exit = IF_ID_register.if_exit;

        //if you move the intrcution you can empty the value out for later use
        fetched_instruction = Instruction(); 
        fetched_instruction.opcode = "";
    }
}