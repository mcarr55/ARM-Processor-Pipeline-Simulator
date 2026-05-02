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

    // Execution State
    int latency = 1;         // IU/FP latency

    int cyclesRemaining = 0; 
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


    long long result_data = 0;


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






//memory controller logic
int memory_busy_counter = 0; // Cycles remaining for current memory task
bool memory_serving_icache = false;
bool memory_serving_dcache = false;

// Statistics
int icache_hits = 0, icache_misses = 0;
int dcache_hits = 0, dcache_misses = 0;

int d_req = 0;
int d_hit = 0;

int memory_bus_busy_until = 0; // The "Shared Bus" clock

struct DCacheLine {
    bool valid = false;
    bool dirty = false;
    int tag = -1;
    int lru_counter = 0; // For LRU replacement
};

// 2 sets, 2 ways per set = 4 blocks total
DCacheLine d_cache[2][2]; 


int dcache_stall_cycles = 0;



//vector address = memory address / 4 (since each instruction is 4 bytes)
vector<Instruction> instruct_list; // Vector to hold the instructions read from the file

//store labels with line number for branch instructions
map<string, int> labels = {
        {"Test", 10},
    };

//store instructions in cycle, keep count with PC and cycle count

//States


//Pipeline registers
Instruction IF_ID_register;

Instruction ID_EX_register;
Instruction ID_FLOAT_EX_register;

Instruction EX_MEM_register;

Instruction MEM_WB_register;

Instruction post_WB_register_info;

bool instruciton_fetched = false;
Instruction fetched_instruction;






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

void moveRegisters();



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

    // R2 points to address 0 (index 0 in data_memory)
    int_registers[2] = 0; 

    // R4 points to address 4 (index 1 in data_memory)
    int_registers[4] = 4; 

    // Clear R3 to ensure we aren't using a "lucky" old value
    int_registers[3] = 0;



    //Add raw line and number id to each instruction for later use in output file
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

        if  (activeHalt ) break; 

        cycle++;

        executeHazardCheck();

        //backwards to 
        // 3. Writeback (WB)
        writeBack();

        //Move IU instuction once each stage
        //MEM_WB_register  = EX_MEM_register;

        // 3. Memory (MEM)
        memory();
        

        //dont move if d cache is stalling
        //if (dcache_stall_cycles == 0) {

        
        // 2. Execute (EX)
        execute();
        

        //if (busy_iu == false){

  
        // 1. Decode (ID)
        decode();    

        if  (activeHalt ) break; 
                
    
        // 0. Fetch (IF)

        /*
        //fetch if nothing has been fetched yet
        if(IF_ID_register.opcode == ""){
            fetch(instruct_list);
        }
            */

        
        fetch(instruct_list);
       
        
        cout << "ex latency " << ID_EX_register.latency << endl;

        

        moveRegisters();




        if (dcache_stall_cycles > 0) dcache_stall_cycles--;
        if (icache_miss_delay > 0) icache_miss_delay--;
   
        //decrease stall count of a instruciton that takes long in execute by 1
        if(ID_EX_register.latency > 0) ID_EX_register.latency--;
        if(ID_FLOAT_EX_register.latency > 0) ID_FLOAT_EX_register.latency--;




        cout << "Cycle: " << cycle 
        << " | dcache_stall: " << dcache_stall_cycles 
        << " | busy_iu: " << busy_iu 
        << " | loadStall: " << loadStall << endl;
        cout << "DEBUG: PC=" << PC  << endl;

    



        debugFunction();




        //move info foward in the pipeline, 

  


    

   
    


        loadStall = false;

        cout << "IF.ID op " << IF_ID_register.opcode << endl;
        cout << "ID.EX op " << ID_EX_register.opcode << endl;
        cout << "EX.MEM op " << EX_MEM_register.opcode << endl;
        cout << "MEM.WB op " << MEM_WB_register.opcode << endl;
        cout  << endl;
        cout  << endl;


        

        // A temporary break so we don't loop forever while testing
        if (cycle == 80 || activeHalt ) break; 
        //  if (cycle > 100 || activeHalt == true) break; 
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



    cout << "int reg 5: " << int_registers[5] << endl;
    cout << "int reg 4: " << int_registers[4] << endl;

    cout << "int reg 3: " << int_registers[3] << endl;
    cout << "int reg 4: " << int_registers[4] << endl;
    cout << "int reg 16: " << int_registers[16] << endl;
    cout << endl;

    

    

    cout << "data mem 0 " << data_memory[0] << endl;



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

    //FP TYPE, Floating Point (FADD, FSUB, FMUL) R[Rd] = R[Rn] + R[Rm]
    else if (checkOpcode(opcode, fp_type)) {

        cout << "FP type instruction found, opcode " << opcode << endl;

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
void fetch(vector<Instruction>& program){

    //Dont move in the case of a fetched intruciton not bing transfered yet, or a miss that needs to wait
    if (instruciton_fetched == true || icache_miss_delay > 0 || activeHalt == true) {
        
        return; 
    }

    // If the PC is within the bounds of instruction vector
    if (PC / 4 < program.size()) {
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
            // MISS: Check if the bus is free, claim if so
            if (cycle >= memory_bus_busy_until) {

                cout << "we missed" << endl;

                // CACHE MISS - Start 12 cycle wait. Do NOT increment PC.
                icache_miss_delay = 11; // 12 cycles total including this one
                i_cache[p.index].valid = true;
                i_cache[p.index].tag = p.tag;
                // The function returns, and next cycle it will check the delay again.
            }
        }
    }
}


//look up values in register file, put them in the ID_EX_register for the next stage
void decode(){

    // grab the register values If it's a real instruction or not EMPTY, 
    if (IF_ID_register.opcode != "") {

        

        /*
        if(IF_ID_register.opcode == "HLT"){

            cout << "detecting halt operation" << endl;
            

            //activeHalt = true;

            return;
        }
        */
        
        

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
            // Example: If the instruction uses 'rn' (first source register)
            // We look up its value in our physical register array      
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

    if (ID_EX_register.latency > 0) return;

    if (ID_EX_register.opcode != "") {
    
        string op = ID_EX_register.opcode;



       
       
        

        // --- LEVEL 3 (IU3) ---
        // MUL finally finishes its 3rd cycle here
        if (op == "MUL") {

            cout << "we are stalling " << op << endl;
            cout << ID_EX_register.latency << endl;

            //check it for latency, if not ready stall 
            if(ID_EX_register.latency > 0){

                return;
            }

            //perform mul operation
            ID_EX_register.result = ID_EX_register.val1 * ID_EX_register.val2;
        } 

        // --- LEVEL 2 (IU2) ---
        else if (op == "ADD" || op == "ADDI" || op == "SUB" || op == "SUBI"){

            cout << "we are stalling " << op << endl;
            cout << ID_EX_register.latency << endl;

            //check it for latency, if not ready stall 
            if(ID_EX_register.latency > 0){


                cout << "returned" << endl;
                return;
            }

            else{

            }


            // ADD and SUB finish their 2nd cycle here
            if (op == "ADD" || op == "ADDI") {
                ID_EX_register.result = ID_EX_register.val1 + ID_EX_register.val2;
                cout << "did math" << endl;
                } 
            if (op == "SUB" || op == "SUBI") {
                ID_EX_register.result = ID_EX_register.val1 - ID_EX_register.val2;
            }

            return;
        }


        // --- LEVEL 1 (IU1) ---
        // Normal 1-cycle ops (AND, ORR, LSL) finish here
        else if (op == "AND" || op == "ANDI" || op == "ORR" || op == "ORRI" || op == "LSL" || op == "LSR"){

            if (op == "AND" || op == "ANDI") {

                ID_EX_register.result = ID_EX_register.val1 & ID_EX_register.val2;
            }

            else if (op == "ORR" || op == "ORRI") {

                ID_EX_register.result = ID_EX_register.val1 | ID_EX_register.val2;
            }

            // Shifts (LSL/LSR)
            else if (op == "LSL") {

                ID_EX_register.result = (uint32_t)ID_EX_register.val1 << ID_EX_register.val2;

                cout << "finished calc" << endl;

                cout <<  "v1 " << ID_EX_register.val1 << "v2 " << ID_EX_register.val2 << endl;
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
                cout << "case 1 " << ID_EX_register.result << endl;
            } else {
                // If shift is 32 or more, we keep the whole thing as shift is out of bounds
                ID_EX_register.result = ID_EX_register.val1;
                cout << "case 2 " << ID_EX_register.result << endl;
            }
        }

        if(op == "B"){

            // 2. Look up the line number in your Map
            // labels["LOOP"] would return something like line 5
            if (labels.find(ID_EX_register.label) != labels.end()) {
                int targetLine = labels[ID_EX_register.label];

                cout << "targetLine is " << targetLine << endl;

                //Update the PC to that line
                PC = targetLine*4 - 4;

                cout << "Branching to LABEL: " << ID_EX_register.label << " at line " << targetLine << endl;

                //copy over other info
    
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

            // 2. Look up the line number in your Map
            // labels["LOOP"] would return something like line 5
            if (labels.find(ID_EX_register.label) != labels.end()) {
                int targetLine = labels[ID_EX_register.label];

                cout << "targetLine is " << targetLine << endl;

                //CBZ jump if zero
                //CBNZ jump if not zero
                if( (op == "CBZ" && ID_EX_register.val1 == 0) || (op == "CBNZ" && ID_EX_register.val1 != 0)){

                    //Update the PC to that line
                    PC = targetLine*4 - 4;

                    //Flush past intrucitons

                    cout << "we jump" << endl;

                    //Initial fetched instruction
                    fetched_instruction = Instruction(); 
                    fetched_instruction.opcode = "";

                
                    //Decode
                    IF_ID_register = Instruction(); 
                    IF_ID_register.opcode = "";

                    return;
                }

                //dont banch but keep moving
                else{
                    cout << "branch is not taken" << endl;

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
            // --- CACHE HIT ---
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
                
                int penalty = 12; // Base: 12 cycles to read the block
                
                // If the block being kicked out is DIRTY, write it back (+12 cycles)
                if (d_cache[set][way].valid && d_cache[set][way].dirty) {
                    penalty += 12;
                }

                memory_bus_busy_until = cycle + penalty;
                dcache_stall_cycles = penalty - 1;

                // Replace and Update metadata
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

       printExits(MEM_WB_register);

       // cout << "rd is " << MEM_WB_register.rd << endl;
        
        string op = MEM_WB_register.opcode;

        if (op == "LDUR" || op == "LDURD"){

            // Safety check: Never write to R31 (the zero register)
            if (MEM_WB_register.rd != 31 && MEM_WB_register.rd != -1) {
                cout << "Writing" << endl;

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
            
            // Safety check: Never write to R31 (the zero register)
            if (MEM_WB_register.rd != 31 && MEM_WB_register.rd != -1) {
                cout << "Writing" << endl;

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

//When testing your WB stage, try printing your registers at the very end of each cycle. 
//You will see that even though ADDI shows up in the trace at Cycle 1, the int_registers[rd] 
//value won't change until the end of Cycle 4 or beginning of Cycle 5.


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




        
    

        /*
        cout << "--- Cycle " << cycle << " ---" << endl;
        cout << "ID_EX_register Opcode: " << ID_EX_register.opcode << endl;
        cout << "ID_EX_register Val1 (Rn): " << ID_EX_register.val1 << endl;
        cout << "ID_EX_register Val2 (Rm/Imm): " << ID_EX_register.val2 << endl;
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
        // Inside  while loop, after all functions and movements:
        cout << "CYCLE " << cycle << " DEBUG:" << endl;
        cout << "  1. IF_ID   Op: " << IF_ID_register.opcode << endl;
        cout << "  2. ID_EX  Op: " << ID_EX_register.opcode << " | V1: " << ID_EX_register.val1 << " | V2: " << ID_EX_register.val2 << endl;
        cout << "  3. IU1_IU2 Op: " << IU1_IU2_register.opcode << " | Res: " << IU1_IU2_register.result << endl;
        */

        /*
        // --- FINAL PIPELINE SNAPSHOT ---
        cout << "================ CYCLE " << cycle << " ================" << endl;
        cout << " [IF]  register: " << IF_ID_register.opcode << endl;

        cout << " [ID]  register: " << ID_EX_register.opcode 
            << " | V1: " << ID_EX_register.val1 
            << " | V2: " << ID_EX_register.val2 << endl;

        cout << " [EX]  register: " << IU1_IU2_register.opcode 
            << " | Result: " << IU1_IU2_register.result << endl;

        cout << " [MEM] register: " << EX_MEM_register.opcode 
            << " | Result: " << EX_MEM_register.result << endl;

        cout << " [WB]  register: " << MEM_WB_register.opcode 
            << " | Writing " << MEM_WB_register.result << " to R" << MEM_WB_register.rd << endl;
        cout << "==========================================" << endl << endl;
        */


}


void printOutputFile(string filename) {
    
    ofstream outFile(filename);

    // Header
    outFile << left << setw(10) << "Cycle Number for each stage   "
            << setw(6) << "IF" << setw(5) << "ID" << setw(9) << "EX" 
            << setw(5) << "MEM" << setw(7) << "  WB" << endl;

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

    string op = ID_EX_register.opcode;

    cout << "EX is looking for R" << ID_EX_register.rn << endl;
    cout << "EX_MEM contains R" << EX_MEM_register.rd << endl;
    cout << "MEM_WB contains R" << MEM_WB_register.rd << endl;

    string float_op = ID_FLOAT_EX_register.opcode;

    

    // --- FP DATA HAZARD (RAW) ---
    // If an instruction in the FP pipeline needs a register currently being loaded or calculated
    if (ID_FLOAT_EX_register.opcode != "") {
        bool hazard = false;
        
        // Check if Source 1 or Source 2 is being written to by instructions ahead in the pipeline
        if (EX_MEM_register.regWrite && (ID_FLOAT_EX_register.rn == EX_MEM_register.rd || ID_FLOAT_EX_register.rm == EX_MEM_register.rd)) {
            hazard = true;
        }
        if (MEM_WB_register.regWrite && (ID_FLOAT_EX_register.rn == MEM_WB_register.rd || ID_FLOAT_EX_register.rm == MEM_WB_register.rd)) {
            hazard = true;
        }

        if (hazard) {
            // Force the instruction to stay in ID by setting a latency stall
            ID_FLOAT_EX_register.latency++; 
        }
    }
    





        //check load use hazards
        if (EX_MEM_register.opcode == "LDUR" || EX_MEM_register.opcode == "LDURD") {

            bool needsLoadData = false;

            // Check if instruction uses the register LDUR is currently fetching
            if (EX_MEM_register.rd != 31) {

                if (EX_MEM_register.rd == ID_EX_register.rn || EX_MEM_register.rd == ID_EX_register.rm) {
                    needsLoadData = true;
                }
                // Special case for STUR: rd is also a source register
                if ((ID_EX_register.opcode == "STUR" || ID_EX_register.opcode == "STUR")  && EX_MEM_register.rd == ID_EX_register.rd) {
                    needsLoadData = true;
                }
            }

            if (needsLoadData) {
                ID_EX_register.latency = 2; // 1 cycle bubble is enough to let LDUR reach MEM_WB
                return; // Stop here; don't perform forwarding yet
            }
        }

        //R[Rd] = R[Rn] + R[Rm]
        if(op == "ADD" || op == "MUL" || op == "SUB" || op == "AND" || op == "ORR"){
            
            //Check Rn (Source 1) 

            //EX/MEM hazard
            if (EX_MEM_register.regWrite && EX_MEM_register.rd != 31 && EX_MEM_register.rd == ID_EX_register.rn) {

                cout << " c1" << endl;
                cout << endl;

                ID_EX_register.val1 = EX_MEM_register.result;

            }

            //MEM/WB hazard
            else if (MEM_WB_register.regWrite && MEM_WB_register.rd != 31 && MEM_WB_register.rd == ID_EX_register.rn) {

                cout << " c2" << endl;
                cout << endl;

                ID_EX_register.val1 = MEM_WB_register.result;
            }
            
            //Check Rm (Source 2).
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

            //Check only Rn. (The second operand is an immediate, so it can't have a data hazard).
            //EX/MEM hazard
            if (EX_MEM_register.regWrite && EX_MEM_register.rd != 31 && EX_MEM_register.rd == ID_EX_register.rn) {

                cout << "cond1" << endl;

                ID_EX_register.val1 = EX_MEM_register.result;

            }

            //MEM/WB hazard
            else if (MEM_WB_register.regWrite && MEM_WB_register.rd != 31 && MEM_WB_register.rd == ID_EX_register.rn) {

                cout << "cond2" << endl;

                ID_EX_register.val1 = MEM_WB_register.result;
            }

        }

        if (op == "STUR" || op == "STURD"){


            //Check Rn (Base address) 
            if (EX_MEM_register.regWrite && EX_MEM_register.rd != 31 && EX_MEM_register.rd == ID_EX_register.rn) {

                ID_EX_register.val1 = EX_MEM_register.result;

            }

            else if (MEM_WB_register.regWrite && MEM_WB_register.rd != 31 && MEM_WB_register.rd == ID_EX_register.rn) {

                ID_EX_register.val1 = MEM_WB_register.result;

            }
            
            
            //Check Rd, source data
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

            // Forward from MEM_WB stage
            else if (MEM_WB_register.regWrite && MEM_WB_register.rd != 31 && MEM_WB_register.rd == ID_EX_register.rn) {

                ID_EX_register.val1 = MEM_WB_register.result;
            }

        } 

        if (op == "MOVK"){

            //Check Rd. 
            if (EX_MEM_register.regWrite && EX_MEM_register.rd != 31 && EX_MEM_register.rd == ID_EX_register.rd) {

                ID_EX_register.val1 = EX_MEM_register.result;

            }
            // Forward from MEM_WB stage
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

        

        cout << "Moving ID_EX to EX_MEM" << endl;
        cout << ID_EX_register.opcode << endl;
        cout << EX_MEM_register.opcode << endl;
        cout << ID_EX_register.latency << endl;
        cout << "result is " << ID_EX_register.result << endl;


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

            cout << "active halt is on in cycle " << cycle << endl;
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