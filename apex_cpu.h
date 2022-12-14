/*
 * apex_cpu.h
 * Contains APEX cpu pipeline declarations
 *
 * Author:
 * Copyright (c) 2020, Gaurav Kothari (gkothar1@binghamton.edu)
 * State University of New York at Binghamton
 */
#ifndef _APEX_CPU_H_
#define _APEX_CPU_H_
#include <stdbool.h>

#include "apex_macros.h"
//new
typedef struct PhyRegfile {
    int front;
    int rear;
    int size;
    unsigned capacity;
    int* array;
}PhyRegfile;

struct Node {
    int data_values[3];
    struct Node* next;
};

typedef struct Register {
    int value;
    int zeroFlag;
    int isValid;
} Register;

/* Format of an APEX instruction  */
typedef struct APEX_Instruction
{
    char opcode_str[128];
    int opcode;
    int rd;
    int rs1;
    int rs2;
    int rs3;
    int imm;
} APEX_Instruction;

typedef struct Bus_forwarding
{
int busy;
int tag_part;
int data_part;
int flag_part;
} Bus_forwarding;

/* Model of CPU stage latch */
typedef struct CPU_Stage
{
    int pc;
    char opcode_str[128];
    int opcode;
    int rs1;
    int rs2;
    int rs3;
    int rd;
    int imm;
    int rs1_value;
    int rs2_value;
    int rs3_value;
    int result_buffer;
    int has_insn;
    int stalled;
    //for IQ
    int free_bit;  //1 allocated 0 free
    int src1_validbit; //initialize to 1 
    int src1_tag;     
    int src1_value;    // value of first source register          rs1
    int src2_validbit;  // initialize to 1
    int src2_tag;
    int src2_value;    // value of second source register         rs2  
    int src3_validbit;  // initialize to 1
    int src3_tag;
    int src3_value;   // value of third source register            rs3 
    int dtype;
    int dest;  //LSQ index or physical register address eg p'8' .  rd
    int prev_dest;
    //ROB variables
    int rob_valid;

    //lsq variables
    int lsq_index;
    int lsq_valid;
    int l_or_s_bit;
    int mem_valid;
    int memory_address;
    int dest_reg_forLOAD;
    //BTB
    int hit_or_miss;
   
} CPU_Stage;

typedef struct ReorderBuffer {
    int front;
    int rear;
    int size;
    unsigned capacity;
    CPU_Stage * array;
}ReorderBuffer;

typedef struct BranchBuffer {
    int front;
    int rear;
    int size;
    unsigned capacity;
    CPU_Stage * array;
}BranchBuffer;

typedef struct LoadStoreFile{
    int front;
    int rear;
    int size;
    unsigned capacity;
    CPU_Stage* array;
}LoadStoreFile;

/* Model of APEX CPU */
typedef struct APEX_CPU
{
    int pc;                        /* Current program counter */
    int clock;                     /* Clock cycles elapsed */
    int insn_completed;            /* Instructions retired */
    int regs[REG_FILE_SIZE];       /* Integer register file */
    int cc[0];                     //cc flag
    int valid_regs[REG_FILE_SIZE];       /* Integer register file indicating register valid bit*/
    int rename_table[REG_FILE_SIZE+1][2];
    Register physicalregs[PREG_FILE_SIZE]; // physical registers- valid bit-0 data value -1 cc flag value -2
    Bus_forwarding bus0;
    Bus_forwarding bus1;
    struct Node* head;
    int code_memory_size;          /* Number of instruction in the input file */
    APEX_Instruction *code_memory; /* Code Memory */
    int data_memory[DATA_MEMORY_SIZE]; /* Data Memory */
    int single_step;               /* Wait for user input after every cycle */
    int zero_flag;                 /* {TRUE, FALSE} Used by BZ and BNZ to branch */
    int fetch_from_next_cycle;
    
    PhyRegfile* freePhysicalRegister;
    ReorderBuffer* reorderBuffer;
    BranchBuffer* branchBuffer;
    LoadStoreFile* loadStoreQueue;
    
    /* Pipeline stages */
    CPU_Stage fetch;
    CPU_Stage decode;
    CPU_Stage dispatch;
    CPU_Stage issueQueue[IQ_SIZE];
    CPU_Stage intFU;
    CPU_Stage mulFU1;
    CPU_Stage mulFU2;
    CPU_Stage mulFU3;
    CPU_Stage mulFU4;
    CPU_Stage logicalopFU;
} APEX_CPU;

APEX_Instruction *create_code_memory(const char *filename, int *size);
APEX_CPU *APEX_cpu_init(const char *filename);
void Forwarding_Bus_0(APEX_CPU *cpu);
void Forwarding_Bus_1(APEX_CPU *cpu);
//ROB
void printROB(APEX_CPU *cpu);

int commit_on_archs(APEX_CPU* cpu);
void APEX_cpu_run(APEX_CPU *cpu,const char *mode ,int cycles);
void APEX_cpu_stop(APEX_CPU *cpu);
//new
int isIssueQueuecompletelyFull(APEX_CPU* cpu);
int isIssueQueueEmpty(APEX_CPU* cpu);

//Physical Register Queue Operations
void assignRegister(struct PhyRegfile* queue, int item);
int retreiveRegister(struct PhyRegfile* queue);

//new
#endif