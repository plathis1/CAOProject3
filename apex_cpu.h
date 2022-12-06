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
typedef struct PhysicalRegistersQueue {
    int front;
    int rear;
    int size;
    unsigned capacity;
    int* array;
}PhysicalRegistersQueue;
struct Node {
    int data_values[2];
    struct Node* next;
};
typedef struct Register {
    int value;
    int zeroFlag;
    int isValid;    //0 - If invalid  1-valid
} Register;

//new end
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
typedef struct Forwarding_Bus
{
int busy;
int tag_part;
int data_part;
int next_data_bus;
int bus_was_busy;
} Forwarding_Bus;
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
    int memory_address;
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

} CPU_Stage;

/* Model of APEX CPU */
typedef struct APEX_CPU
{
    int pc;                        /* Current program counter */
    int clock;                     /* Clock cycles elapsed */
    int insn_completed;            /* Instructions retired */
    int regs[REG_FILE_SIZE];       /* Integer register file */
    int valid_regs[REG_FILE_SIZE];       /* Integer register file indicating register valid bit*/
    int rename_table[REG_FILE_SIZE+1];
    Register physicalregs[PREG_FILE_SIZE]; // physical registers- valid bit-0 data value -1 cc flag value -2
    Forwarding_Bus bus0;
    Forwarding_Bus bus1;
    struct Node* head;
    int code_memory_size;          /* Number of instruction in the input file */
    APEX_Instruction *code_memory; /* Code Memory */
    int data_memory[DATA_MEMORY_SIZE]; /* Data Memory */
    int single_step;               /* Wait for user input after every cycle */
    int zero_flag;                 /* {TRUE, FALSE} Used by BZ and BNZ to branch */
    int fetch_from_next_cycle;
    
    PhysicalRegistersQueue* freePhysicalRegister;
    int allocationList[15];
    

    /* Pipeline stages */
    CPU_Stage fetch;
    CPU_Stage decode;
    CPU_Stage dispatch;
    CPU_Stage issueQueue[8];
    CPU_Stage intFU;
    CPU_Stage mulFU;
    CPU_Stage logicalopFU;
} APEX_CPU;

APEX_Instruction *create_code_memory(const char *filename, int *size);
APEX_CPU *APEX_cpu_init(const char *filename);
void Forwarding_Bus_0_tagpart(APEX_CPU *cpu);
void Forwarding_Bus_1_tagpart(APEX_CPU *cpu);
void APEX_cpu_run(APEX_CPU *cpu);
void APEX_cpu_stop(APEX_CPU *cpu);
//new
int isIssueQueueFull(APEX_CPU* cpu);
int isIssueQueueEmpty(APEX_CPU* cpu);
void pushToIssueQueue(APEX_CPU* cpu,CPU_Stage instruction);
void fetchNextIntegerInstructionFromIssueQueue(APEX_CPU* cpu);
void fetchNextMultiplyInstructionFromIssueQueue(APEX_CPU* cpu);
void printContentsOfIssueQueue(APEX_CPU* cpu);

//Physical Register Queue Operations
int isRegisterQueueFull(struct PhysicalRegistersQueue* queue);
int isRegisterQueueEmpty(struct PhysicalRegistersQueue* queue);
void insertRegister(struct PhysicalRegistersQueue* queue, int item);
int getRegister(struct PhysicalRegistersQueue* queue);
int registerQueueFront(struct PhysicalRegistersQueue* queue);
int registerQueueRear(struct PhysicalRegistersQueue* queue);

//new
#endif