/*
 * apex_cpu.c
 * Contains APEX cpu pipeline implementation
 *
 * Author:
 * Copyright (c) 2020, Gaurav Kothari (gkothar1@binghamton.edu)
 * State University of New York at Binghamton
 */
#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include "apex_cpu.h"

#include "apex_macros.h"

// ROB
int index_ins = 100; // started at 100 increases by 5
int array2[3];
struct ReorderBuffer *establishROBQueueByCapacity(unsigned capacity)
{
  struct ReorderBuffer *rob = (struct ReorderBuffer *)malloc(sizeof(struct ReorderBuffer));
  rob->capacity = capacity;
  rob->front = rob->size = 0;
  rob->rear = capacity - 1;
  rob->array = (CPU_Stage *)malloc(rob->capacity * sizeof(CPU_Stage));
  return rob;
}
struct BranchBuffer *establishBTBQueueByCapacity(unsigned capacity)
{
  struct BranchBuffer *btb = (struct BranchBuffer *)malloc(sizeof(struct BranchBuffer));
  btb->capacity = capacity;
  btb->front = btb->size = 0;
  btb->rear = capacity - 1;
  btb->array = (CPU_Stage *)malloc(btb->capacity * sizeof(CPU_Stage));
  return btb;
}
struct LoadStoreFile *establishLSQQueueByCapacity(unsigned capacity)
{
  struct LoadStoreFile *lsq = (struct LoadStoreFile *)malloc(sizeof(struct LoadStoreFile));
  lsq->capacity = capacity;
  lsq->front = lsq->size = 0;
  lsq->rear = capacity - 1;
  lsq->array = (CPU_Stage *)malloc(lsq->capacity * sizeof(CPU_Stage));
  return lsq;
}
struct PhyRegfile *establishQueueByCapacity(unsigned capacity)
{

  struct PhyRegfile *queue = (struct PhyRegfile *)malloc(sizeof(struct PhyRegfile));
  queue->capacity = capacity;
  queue->front = queue->size = 0;
  queue->rear = capacity - 1;
  queue->array = (int *)malloc(queue->capacity * sizeof(int));
  return queue;
}
// list
void add_Newdata(struct Node **head_ref_cpu, int new_data[])
{
  struct Node *new_node = (struct Node *)malloc(sizeof(struct Node));
  struct Node *last = *head_ref_cpu;
  new_node->data_values[0] = new_data[0];
  new_node->data_values[1] = new_data[1];
  new_node->data_values[2] = new_data[2];
  new_node->next = NULL;
  if (*head_ref_cpu == NULL)
  {
    *head_ref_cpu = new_node;
    return;
  }
  for (; last->next != NULL; last = last->next)
    continue;

  last->next = new_node;
  return;
}

int *removeDataAtPosiiton(struct Node **head_ref_cpu, int *returnarray)
{

  struct Node *temp = *head_ref_cpu;

  if (temp != NULL)
  {
    returnarray[0] = (*head_ref_cpu)->data_values[0];
    returnarray[1] = (*head_ref_cpu)->data_values[1];
    returnarray[2] = (*head_ref_cpu)->data_values[2];
    *head_ref_cpu = temp->next;
    free(temp);
    return returnarray;
  }
  if (temp == NULL)
    return returnarray;

  free(temp);
  return returnarray;
}

void printList(struct Node *node)
{
  for (; node != NULL; node = node->next)
  {
    printf("Tag part in list%d ", node->data_values[0]);
    printf("Data part in list%d \n", node->data_values[1]);
  }
}
void print_bus0(APEX_CPU *cpu)
{
  printf("Bus 0 have values %d, %d and %d\n", cpu->bus0.tag_part, cpu->bus0.data_part, cpu->bus0.flag_part);
}

void print_bus1(APEX_CPU *cpu)
{
  printf("Bus 1 have values %d, %d and %d\n", cpu->bus1.tag_part, cpu->bus1.data_part, cpu->bus1.flag_part);
}

int print_physical_register_state(APEX_CPU *cpu)
{
  printf("\n=============== STATE OF PHYSICAL ARCHITECTURAL REGISTER FILE ==========\n");
  int count = 0;
  for (; count < PREG_FILE_SIZE; count++)
  {
    printf("|  P[%d]  | V-bit = %s  | Value = %d  | CC = %d  \n", count, (!cpu->physicalregs[count].isValid ? "VALID" : "INVALID"), cpu->physicalregs[count].value, cpu->physicalregs[count].zeroFlag);
  }
  return 0;
}

int print_rename_table(APEX_CPU *cpu)
{
  printf("Rename table contents are:\n");
  int i = 0;
  for (i = 0; i < REG_FILE_SIZE; i++)
  {
    if (cpu->rename_table[i][0] != -1)
    {
      printf("R[%d] --> P%d ---->and value=%d\n", i, cpu->rename_table[i][0], cpu->physicalregs[cpu->rename_table[i][0]].value);
    }
  }
  printf("R[%d] --> CC ---->and value=%d\n", i, cpu->physicalregs[cpu->rename_table[i][0]].zeroFlag);
  return 0;
}

static int get_code_memory_index_from_pc(const int pc)
{
  return (pc - 4000) / 4;
}

static void print_instruction(const CPU_Stage *stage)
{
  switch (stage->opcode)
  {
  case OPCODE_ADD:
  case OPCODE_SUB:
  case OPCODE_MUL:
  case OPCODE_DIV:
  case OPCODE_AND:
  case OPCODE_OR:
  case OPCODE_XOR:
  case OPCODE_LDR:
  {
    printf("%s,R%d,R%d,R%d ", stage->opcode_str, stage->rd, stage->rs1,
           stage->rs2);
    break;
  }
  case OPCODE_ADDL:
  case OPCODE_SUBL:
  case OPCODE_LOAD:
  {
    printf("%s,R%d,R%d,#%d ", stage->opcode_str, stage->rd, stage->rs1, stage->imm);
    break;
  }

  case OPCODE_MOVC:
  {
    printf("%s,R%d,#%d ", stage->opcode_str, stage->rd, stage->imm);
    break;
  }
  case OPCODE_JUMP:
  {
    printf("%s,R%d,#%d ", stage->opcode_str, stage->rs1, stage->imm);
    break;
  }
  case OPCODE_STORE:
  {
    printf("%s,R%d,R%d,#%d ", stage->opcode_str, stage->rs1, stage->rs2,
           stage->imm);
    break;
  }

  case OPCODE_STR:
  {
    printf("%s,R%d,R%d,R%d ", stage->opcode_str, stage->rs1, stage->rs2,
           stage->rs3);
    break;
  }

  case OPCODE_BZ:
  case OPCODE_BNZ:
  {
    printf("%s,#%d ", stage->opcode_str, stage->imm);
    break;
  }

  case OPCODE_HALT:
  case OPCODE_NOP:
  {
    printf("%s", stage->opcode_str);
    break;
  }

  case OPCODE_CMP:
  {
    printf("%s,R%d,R%d,R%d ", stage->opcode_str, stage->rd, stage->rs1, stage->rs2);
    break;
  }
  }
}
static void print_fu_unit(const char *name, const CPU_Stage *stage)
{
  printf("Instruction at %-18s---> pc(%d) ", name, stage->pc);
  switch (stage->opcode)
  {
  case OPCODE_ADD:
  case OPCODE_SUB:
  case OPCODE_MUL:
  case OPCODE_DIV:
  case OPCODE_AND:
  case OPCODE_OR:
  case OPCODE_XOR:
  case OPCODE_LDR:
  {
    printf("%s,P%d,P%d,P%d ", stage->opcode_str, stage->dest, stage->src1_tag, stage->src2_tag);
    break;
  }
  case OPCODE_ADDL:
  case OPCODE_SUBL:
  case OPCODE_LOAD:
  {
    printf("%s,P%d,P%d,#%d ", stage->opcode_str, stage->dest, stage->src1_tag, stage->src2_tag);
    break;
  }

  case OPCODE_MOVC:
  {
    printf("%s,P%d,#%d ", stage->opcode_str, stage->dest, stage->imm);
    break;
  }
  case OPCODE_JUMP:
  {
    printf("%s,P%d,#%d ", stage->opcode_str, stage->src1_tag, stage->imm);
    break;
  }
  case OPCODE_STORE:
  {
    printf("%s,P%d,P%d,#%d ", stage->opcode_str, stage->src1_tag, stage->src2_tag, stage->imm);
    break;
  }

  case OPCODE_STR:
  {
    printf("%s,P%d,P%d,P%d ", stage->opcode_str, stage->src1_tag, stage->src2_tag, stage->src3_tag);
    break;
  }

  case OPCODE_BZ:
  case OPCODE_BNZ:
  {
    printf("%s,#%d ", stage->opcode_str, stage->imm);
    break;
  }

  case OPCODE_HALT:
  case OPCODE_NOP:
  {
    printf("%s", stage->opcode_str);
    break;
  }

  case OPCODE_CMP:
  {
    printf("%s,P%d,P%d,P%d ", stage->opcode_str, stage->dest, stage->src1_tag, stage->src2_tag);
    break;
  }
  }
  printf("\n");
}

static void display_issueQueue(APEX_CPU *cpu)
{
  if (isIssueQueueEmpty(cpu) == 0)
  {
    printf("\n");
    printf("IQ contents are:\n");
  }
  for (int i = 0; i < IQ_SIZE; i++)
  {
    if (cpu->issueQueue[i].free_bit == 1)
    {
      switch (cpu->issueQueue[i].opcode)
      {
      case OPCODE_ADD:
      case OPCODE_SUB:
      case OPCODE_MUL:
      case OPCODE_DIV:
      case OPCODE_AND:
      case OPCODE_OR:
      case OPCODE_XOR:
      case OPCODE_LDR:
      {
        printf("%s,P%d,P%d,P%d ", cpu->issueQueue[i].opcode_str, cpu->issueQueue[i].dest, cpu->issueQueue[i].rs1,
               cpu->issueQueue[i].rs2);
        break;
      }
      case OPCODE_ADDL:
      case OPCODE_SUBL:
      case OPCODE_LOAD:
      {
        printf("%s,P%d,P%d,#%d ", cpu->issueQueue[i].opcode_str, cpu->issueQueue[i].dest, cpu->issueQueue[i].rs1,
               cpu->issueQueue[i].imm);
        break;
      }

      case OPCODE_MOVC:
      {
        printf("%s,P%d,#%d ", cpu->issueQueue[i].opcode_str, cpu->issueQueue[i].dest, cpu->issueQueue[i].imm);
        break;
      }
      case OPCODE_JUMP:
      {
        printf("%s,P%d,#%d ", cpu->issueQueue[i].opcode_str, cpu->issueQueue[i].rs1, cpu->issueQueue[i].imm);
        break;
      }
      case OPCODE_STORE:
      {
        printf("%s,P%d,P%d,#%d ", cpu->issueQueue[i].opcode_str, cpu->issueQueue[i].rs1, cpu->issueQueue[i].rs2,
               cpu->issueQueue[i].imm);
        break;
      }

      case OPCODE_STR:
      {
        printf("%s,P%d,P%d,P%d ", cpu->issueQueue[i].opcode_str, cpu->issueQueue[i].rs1, cpu->issueQueue[i].rs2,
               cpu->issueQueue[i].rs3);
        break;
      }

      case OPCODE_BZ:
      case OPCODE_BNZ:
      {
        printf("%s,#%d ", cpu->issueQueue[i].opcode_str, cpu->issueQueue[i].imm);
        break;
      }

      case OPCODE_HALT:
      case OPCODE_NOP:
      {
        printf("%s", cpu->issueQueue[i].opcode_str);
        break;
      }

      case OPCODE_CMP:
      {
        printf("%s,P%d,P%d,P%d ", cpu->issueQueue[i].opcode_str, cpu->issueQueue[i].rd, cpu->issueQueue[i].rs1, cpu->issueQueue[i].rs2);
        break;
      }
      }
      printf("\n");
    }
  }
}

int print_register_state(APEX_CPU *cpu)
{
  printf("\n=============== STATE OF ARCHITECTURAL REGISTER FILE ==========\n");
  int no_of_ins = 0;
  int no_registers = REG_FILE_SIZE;
  for (; no_of_ins < no_registers; no_of_ins++)
  {
    printf("| \t REG[%d] \t | \t Value = %d \t | \t Status = %s \t \n", no_of_ins, cpu->regs[no_of_ins], (!cpu->valid_regs[no_of_ins] ? "VALID" : "INVALID"));
  }
  return 0;
}

int print_data_memory(APEX_CPU *cpu)
{
  printf("\n=============== STATE OF DATA MEMORY =============\n");
  printf("\n");
  int no_of_ins = 0;
  int count = 0;
  for (; no_of_ins < 999; no_of_ins++)
  {
    if (cpu->data_memory[no_of_ins] != 0)
    {
      printf("MEM[%d]  = %d\n", no_of_ins, cpu->data_memory[no_of_ins]);
      count++;
    }
  }
  printf("\n");
  if (count == 0)
  {
    printf("No non-zero memory values exists\n\n");
  }
  return 0;
}

/* Debug function which prints the CPU stage content
 *
 * Note: You can edit this function to print in more detail
 */
static void print_stage_content(const char *name, const CPU_Stage *stage)
{
  printf("Instruction at %-18s---> pc(%d) ", name, stage->pc);
  print_instruction(stage);
  printf("\n");
}

static void print_stage_empty(const char *name)
{
  printf("Instruction at %-18s---> Empty ", name);
  printf("\n");
}

/* Debug function which prints the register file
 *
 * Note: You are not supposed to edit this function
 */
static void print_reg_file(const APEX_CPU *cpu)
{
  int i;

  printf("----------------------------\n%s\n----------------------------\n", " Architectural Registers:");

  for (int i = 0; i < REG_FILE_SIZE / 2; ++i)
  {
    printf("R%-3d[%-3d] ", i, cpu->regs[i]);
  }

  printf("\n");

  for (i = (REG_FILE_SIZE / 2); i < REG_FILE_SIZE; ++i)
  {
    printf("R%-3d[%-3d] ", i, cpu->regs[i]);
  }

  printf("\n");
}

/*
 * Fetch Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
static void APEX_fetch(APEX_CPU *cpu)
{

  APEX_Instruction *current_ins;

  if (cpu->fetch.has_insn)
  {
    /* This fetches new branch target instruction from next cycle */
    if (cpu->fetch_from_next_cycle == TRUE)
    {

      cpu->fetch_from_next_cycle = FALSE;
      if (ENABLE_DEBUG_MESSAGES)
      {
        print_stage_empty("Fetch");
      }

      return;
    }

    /* Store current PC in fetch latch */
    cpu->fetch.pc = cpu->pc;

    /* Index into code memory using this pc and copy all instruction fields
     * into fetch latch  */
    current_ins = &cpu->code_memory[get_code_memory_index_from_pc(cpu->pc)];
    strcpy(cpu->fetch.opcode_str, current_ins->opcode_str);
    cpu->fetch.opcode = current_ins->opcode;
    cpu->fetch.rd = current_ins->rd;
    cpu->fetch.rs1 = current_ins->rs1;
    cpu->fetch.rs2 = current_ins->rs2;
    cpu->fetch.rs3 = current_ins->rs3;
    cpu->fetch.imm = current_ins->imm;

    switch (cpu->fetch.opcode)
    {
    case OPCODE_BNZ:
    case OPCODE_BZ:
    {
      if (cpu->branchBuffer->size != cpu->branchBuffer->capacity)
      {
        int count = 0;
        for (int i = cpu->branchBuffer->front; i <= cpu->branchBuffer->rear; i++)
        {
          if (cpu->branchBuffer->array[i].pc == cpu->fetch.pc)
          {
            count = 1;
          }
        }
        if (count == 0)
        {
          cpu->branchBuffer->rear = (cpu->branchBuffer->rear + 1) % cpu->branchBuffer->capacity;
          cpu->branchBuffer->array[cpu->branchBuffer->rear] = cpu->fetch;
          cpu->branchBuffer->size = cpu->branchBuffer->size + 1;
        }
      }

      break;
    }

    default:
    {
      break;
    }
    }

    if (cpu->decode.stalled == 0)
    {
      /* Update PC for next instruction */
      cpu->pc += 4;
      switch (cpu->fetch.opcode)
      {
      case OPCODE_BNZ:
      case OPCODE_BZ:
      {
        for (int i = cpu->branchBuffer->front; i <= cpu->branchBuffer->rear; i++)
        {
          if (cpu->branchBuffer->array[i].pc == cpu->fetch.pc)
          {
            if (cpu->branchBuffer->array[i].hit_or_miss == 1)
            {
              cpu->pc = cpu->pc + cpu->fetch.imm - 4;
            }
          }
        }
      }
      default:
      {
        break;
      }
      }

      /* Copy data from fetch latch to decode latch*/
      cpu->decode = cpu->fetch;
    }
    else
    {
      (cpu->fetch.stalled = 1);
    }

    if (ENABLE_DEBUG_MESSAGES)
    {
      print_stage_content("Fetch", &cpu->fetch);
    }

    /* Stop fetching new instructions if HALT is fetched */
    if (cpu->fetch.opcode == OPCODE_HALT && cpu->decode.stalled == 0)
    {
      cpu->decode = cpu->fetch;
      cpu->fetch.has_insn = FALSE;
    }
  }
  else if (cpu->decode.stalled == 0 && cpu->fetch.opcode != OPCODE_HALT)
  {
    /* Copy data from fetch latch to decode latch*/
    cpu->decode = cpu->fetch;
  }
  else
  {
    cpu->fetch.has_insn = FALSE;
    if (ENABLE_DEBUG_MESSAGES)
    {
      print_stage_empty("Fetch");
    }
  }
}
/*
 * Decode Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
static void APEX_decode(APEX_CPU *cpu)
{
  if (cpu->freePhysicalRegister->size == 0)
  {
    cpu->decode.stalled = 1; // set this flag to 0 in the code
  }
  if (cpu->decode.has_insn && cpu->decode.stalled == 0)
  {

    switch (cpu->decode.opcode)
    {
    case OPCODE_ADD:
    case OPCODE_SUB:
    case OPCODE_DIV:
    case OPCODE_LDR:
    case OPCODE_CMP:
    case OPCODE_MUL:
    case OPCODE_AND:
    case OPCODE_OR:
    case OPCODE_XOR:
    {
      // step1 allocate a free physical register for destination register
      int free = retreiveRegister(cpu->freePhysicalRegister);

      // step2 lookup the rename table for most recent instances of source registers
      // src1

      cpu->decode.src1_tag = cpu->rename_table[cpu->decode.rs1][0];
      cpu->decode.src1_validbit = cpu->physicalregs[cpu->rename_table[cpu->decode.rs1][0]].isValid;
      cpu->decode.src1_value = cpu->physicalregs[cpu->rename_table[cpu->decode.rs1][0]].value;

      // src2
      cpu->decode.src2_validbit = cpu->physicalregs[cpu->rename_table[cpu->decode.rs2][0]].isValid;
      cpu->decode.src2_tag = cpu->rename_table[cpu->decode.rs2][0];
      cpu->decode.src2_value = cpu->physicalregs[cpu->rename_table[cpu->decode.rs2][0]].value;
      
      cpu->decode.prev_dest = cpu->rename_table[cpu->decode.rd][0];
      
      if(free==cpu->decode.src1_tag){
        cpu->decode.src1_validbit = cpu->physicalregs[cpu->rename_table[cpu->decode.prev_dest][0]].isValid;
        cpu->decode.src1_value = cpu->physicalregs[cpu->rename_table[cpu->decode.prev_dest][0]].value;
       } if(free==cpu->decode.src2_tag){
        cpu->decode.src2_validbit = cpu->physicalregs[cpu->rename_table[cpu->decode.prev_dest][0]].isValid;
        cpu->decode.src2_value = cpu->physicalregs[cpu->rename_table[cpu->decode.prev_dest][0]].value;
       }if(cpu->rename_table[cpu->decode.rs1][1]==0){
        cpu->decode.src1_validbit = 0;
        cpu->decode.src1_value = cpu->regs[cpu->decode.rs1];
       } if(cpu->rename_table[cpu->decode.rs2][1]==0){
        cpu->decode.src2_validbit = 0;
        cpu->decode.src2_value = cpu->regs[cpu->decode.rs2];
       
       }

       
        


      // step 3 update latest instance of rd in rename table
      cpu->rename_table[cpu->decode.rd][0] = free;
      cpu->rename_table[cpu->decode.rd][1]=1;
      cpu->decode.dest = free;
      cpu->physicalregs[free].isValid = 1;

      break;
    }
    case OPCODE_STORE:
    {

      // step2 lookup the rename table for most recent instances of source registers
      // src1
      cpu->decode.src1_validbit = cpu->physicalregs[cpu->rename_table[cpu->decode.rs1][0]].isValid;
      cpu->decode.src1_tag = cpu->rename_table[cpu->decode.rs1][0];
      cpu->decode.src1_value = cpu->physicalregs[cpu->rename_table[cpu->decode.rs1][0]].value;
      // src2
      cpu->decode.src2_validbit = cpu->physicalregs[cpu->rename_table[cpu->decode.rs2][0]].isValid;
      cpu->decode.src2_tag = cpu->rename_table[cpu->decode.rs2][0];
      cpu->decode.src2_value = cpu->physicalregs[cpu->rename_table[cpu->decode.rs2][0]].value;
      if(cpu->rename_table[cpu->decode.rs1][1]==0){
        cpu->decode.src1_validbit = 0;
        cpu->decode.src1_value = cpu->regs[cpu->decode.rs1];
       } if(cpu->rename_table[cpu->decode.rs2][1]==0){
        cpu->decode.src2_validbit = 0;
        cpu->decode.src2_value = cpu->regs[cpu->decode.rs2];
       }
      break;
    }
    case OPCODE_MOVC:
    {
      int free = retreiveRegister(cpu->freePhysicalRegister);

      cpu->decode.prev_dest = cpu->rename_table[cpu->decode.rd][0];
      cpu->rename_table[cpu->decode.rd][0] = free;
      cpu->decode.dest = free;
      cpu->physicalregs[free].isValid = 1;
      cpu->rename_table[cpu->decode.rd][1]=1;
      /* MOVC doesn't have register operands */
      break;
    }
    case OPCODE_JUMP:
    {
      // step 2
      // src1
      cpu->decode.src1_validbit = cpu->physicalregs[cpu->rename_table[cpu->decode.rs1][0]].isValid;
      cpu->decode.src1_tag = cpu->rename_table[cpu->decode.rs1][0];
      cpu->decode.src1_value = cpu->physicalregs[cpu->rename_table[cpu->decode.rs1][0]].value;
      cpu->fetch.has_insn = TRUE;

      break;
    }

    case OPCODE_ADDL:
    case OPCODE_SUBL:
    case OPCODE_LOAD:
    {
      // step 1
      int free = retreiveRegister(cpu->freePhysicalRegister);
      // step 2
      // src1
      cpu->decode.src1_validbit = cpu->physicalregs[cpu->rename_table[cpu->decode.rs1][0]].isValid;
      cpu->decode.src1_tag = cpu->rename_table[cpu->decode.rs1][0];
      cpu->decode.src1_value = cpu->physicalregs[cpu->rename_table[cpu->decode.rs1][0]].value;
      cpu->decode.prev_dest = cpu->rename_table[cpu->decode.rd][0];
      if(free==cpu->decode.src1_tag){
        cpu->decode.src1_validbit = cpu->physicalregs[cpu->rename_table[cpu->decode.prev_dest][0]].isValid;
        cpu->decode.src1_value = cpu->physicalregs[cpu->rename_table[cpu->decode.prev_dest][0]].value;
       } 
      if(cpu->rename_table[cpu->decode.rs1][1]==0){
        cpu->decode.src1_validbit = 0;
        cpu->decode.src1_value = cpu->regs[cpu->decode.rs1];
       } 
      // step 3
      
      cpu->rename_table[cpu->decode.rd][0] = free;
      cpu->decode.dest = free;
      cpu->physicalregs[free].isValid = 1;
      cpu->rename_table[cpu->decode.rd][1]=1;
      break;
    }

    case OPCODE_STR:
    {

      // src1
      cpu->decode.src1_validbit = cpu->physicalregs[cpu->rename_table[cpu->decode.rs1][0]].isValid;
      cpu->decode.src1_tag = cpu->rename_table[cpu->decode.rs1][0];
      cpu->decode.src1_value = cpu->physicalregs[cpu->rename_table[cpu->decode.rs1][0]].value;
      // src2
      cpu->decode.src2_validbit = cpu->physicalregs[cpu->rename_table[cpu->decode.rs2][0]].isValid;
      cpu->decode.src2_tag = cpu->rename_table[cpu->decode.rs2][0];
      cpu->decode.src2_value = cpu->physicalregs[cpu->rename_table[cpu->decode.rs2][0]].value;
      // src3
      cpu->decode.src3_validbit = cpu->physicalregs[cpu->rename_table[cpu->decode.rs3][0]].isValid;
      cpu->decode.src3_tag = cpu->rename_table[cpu->decode.rs3][0];
      cpu->decode.src3_value = cpu->physicalregs[cpu->rename_table[cpu->decode.rs3][0]].value;
      if(cpu->rename_table[cpu->decode.rs1][1]==0){
        cpu->decode.src1_validbit = 0;
        cpu->decode.src1_value = cpu->regs[cpu->decode.rs1];
       } if(cpu->rename_table[cpu->decode.rs2][1]==0){
        cpu->decode.src2_validbit = 0;
        cpu->decode.src2_value = cpu->regs[cpu->decode.rs2];
       }
       if(cpu->rename_table[cpu->decode.rs3][1]==0){
        cpu->decode.src3_validbit = 0;
        cpu->decode.src3_value = cpu->regs[cpu->decode.rs3];
       } 

      break;
    }

    case OPCODE_BZ:
    {
      /* BZ doesn't have register operands */
      cpu->decode.rd = -1;
      break;
    }

    case OPCODE_BNZ:
    {
      /* BNZ doesn't have register operands */
      cpu->decode.rd = -1;
      break;
    }

    case OPCODE_HALT:
    {
      /* HALT doesn't have register operands */
      cpu->decode.rd = -1;
      break;
    }

    case OPCODE_NOP:
    {
      /* NOP doesn't have register operands */
      cpu->decode.rd = -1;
      break;
    }

    default:
    {
      break;
    }
    }
    if (cpu->decode.src1_tag == -1)
    {
      cpu->decode.src1_tag = cpu->decode.rs1;
      cpu->decode.src1_value = cpu->decode.rs1_value;
      cpu->decode.src1_validbit = 0;
    }
    if (cpu->decode.src2_tag == -1)
    {
      cpu->decode.src2_tag = cpu->decode.rs2;
      cpu->decode.src2_value = cpu->decode.rs2_value;
      cpu->decode.src2_validbit = 0;
    }
    if (cpu->decode.src3_tag == -1)
    {
      cpu->decode.src3_tag = cpu->decode.rs3;
      cpu->decode.src3_value = cpu->decode.rs3_value;
      cpu->decode.src3_validbit = 0;
    }
    if (cpu->dispatch.stalled == 0 && cpu->decode.stalled == 0)
    {
      /* Copy data from decode latch to dispatch latch*/
      cpu->dispatch = cpu->decode;

      if (ENABLE_DEBUG_MESSAGES)
      {
        print_stage_content("Decode/Rename1", &cpu->decode);
      }
    }
    else
    {
      if (ENABLE_DEBUG_MESSAGES)
      {
        if (cpu->decode.has_insn == FALSE)
        {
        }
        else
        {
          print_stage_content("Decode/Rename1", &cpu->decode);
        }
      }
    }
  }
  else
  {
    if (ENABLE_DEBUG_MESSAGES)
    {
      if (!(cpu->dispatch.stalled || isIssueQueuecompletelyFull(cpu)))
      {
        cpu->dispatch = cpu->decode;
      }
      else
      {
        cpu->decode.stalled = 1;
      }
      if (cpu->decode.has_insn == FALSE)
      {
        print_stage_empty("Decode/Rename1");
      }
      else
      {
        print_stage_content("Decode/Rename1", &cpu->decode);
      }
    }
  }
}
static void APEX_dispatch(APEX_CPU *cpu)
{
  if (cpu->dispatch.has_insn && cpu->dispatch.stalled == 0 && isIssueQueuecompletelyFull(cpu) == 0 && (cpu->reorderBuffer->size != cpu->reorderBuffer->capacity) && (cpu->loadStoreQueue->size != cpu->loadStoreQueue->capacity))
  {
    switch (cpu->dispatch.opcode)
    {
    case OPCODE_ADD:
    case OPCODE_SUB:
    case OPCODE_DIV:
    case OPCODE_LDR:
    case OPCODE_STORE:
    case OPCODE_CMP:
    {

      // prepare IQ if an entry is available
      cpu->dispatch.free_bit = 1;      // allocated
      cpu->dispatch.src3_validbit = 0; // not using r3 so made it valid
      cpu->dispatch.src3_tag = 0;
      cpu->dispatch.dtype = 1;
      break;
    }
    case OPCODE_MUL:
    {
      // prepare IQ if an entry is available
      cpu->dispatch.free_bit = 1;
      cpu->dispatch.src3_validbit = 0; // not using r3 so made it valid
      cpu->dispatch.src3_tag = 0;
      cpu->dispatch.dtype = 1;
      break;
    }
    case OPCODE_AND:
    case OPCODE_OR:
    case OPCODE_XOR:
    {

      // prepare IQ if an entry is available
      cpu->dispatch.free_bit = 1;
      cpu->dispatch.src3_validbit = 0; // not using r3 so made it valid
      cpu->dispatch.src3_tag = 0;
      cpu->dispatch.dtype = 1;
      break;
    }

    case OPCODE_MOVC:
    {
      /* MOVC doesn't have register operands */
      // prepare IQ if an entry is available
      cpu->dispatch.free_bit = 1;
      cpu->dispatch.src1_validbit = 0;
      cpu->dispatch.src1_tag = 0;
      cpu->dispatch.src2_validbit = 0;
      cpu->dispatch.src1_tag = 0;
      cpu->dispatch.src3_validbit = 0;
      cpu->dispatch.src3_tag = 0;
      cpu->dispatch.dtype = 1;
      break;
    }
    case OPCODE_JUMP:
    {
      // prepare IQ if an entry is available
      cpu->dispatch.free_bit = 1;
      cpu->dispatch.src2_validbit = 0; // initialized to valid
      cpu->dispatch.src1_tag = 0;
      cpu->dispatch.src3_validbit = 0; // not using r3 so made it valid
      cpu->dispatch.src3_tag = 0;
      cpu->dispatch.dtype = 1;
      cpu->decode.has_insn = TRUE;
      break;
    }

    case OPCODE_ADDL:
    case OPCODE_SUBL:
    case OPCODE_LOAD:
    {
      // prepare IQ if an entry is available
      cpu->dispatch.free_bit = 1;
      cpu->dispatch.src2_validbit = 0; // not using r2 so make it valid
      cpu->dispatch.src2_tag = 0;
      cpu->dispatch.src3_validbit = 0; // not using r3 so made it valid
      cpu->dispatch.src3_tag = 0;
      cpu->dispatch.dtype = 1;
      break;
    }

    case OPCODE_STR:
    {

      // prepare IQ if an entry is available
      cpu->dispatch.free_bit = 1;
      cpu->dispatch.dtype = 1;
      cpu->dispatch.dest = -1;

      break;
    }

    case OPCODE_BZ:
    {
      /* BZ doesn't have register operands */
      cpu->dispatch.free_bit = 1;
      break;
    }

    case OPCODE_BNZ:
    {
      /* BNZ doesn't have register operands */
      cpu->dispatch.free_bit = 1;
      break;
    }

    case OPCODE_HALT:
    {
      /* HALT doesn't have register operands */
      cpu->dispatch.free_bit = 1;
      cpu->decode.has_insn = 0;
      break;
    }

    case OPCODE_NOP:
    {
      /* NOP doesn't have register operands */
      cpu->dispatch.free_bit = 1;
      break;
    }

    default:
    {
      break;
    }
    }

    if (cpu->dispatch.stalled == 0 && isIssueQueuecompletelyFull(cpu) == 0 && (cpu->reorderBuffer->size != cpu->reorderBuffer->capacity))
    {
      int free_IQ = -1;
      for (int i = 0; i < IQ_SIZE; i++)
      {
        if (cpu->issueQueue[i].free_bit == 0)
        {
          free_IQ = i;
          break;
        }
      }

      if (cpu->dispatch.opcode == OPCODE_LOAD || cpu->dispatch.opcode == OPCODE_STORE ||
          cpu->dispatch.opcode == OPCODE_LDR || cpu->dispatch.opcode == OPCODE_STR)
      {
        if (cpu->loadStoreQueue->size != cpu->loadStoreQueue->capacity)
        {
          cpu->loadStoreQueue->rear = (cpu->loadStoreQueue->rear + 1) % cpu->loadStoreQueue->capacity;
          cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear] = cpu->dispatch;
          cpu->loadStoreQueue->size = cpu->loadStoreQueue->size + 1;
          cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].lsq_index = index_ins;
          index_ins = index_ins + 5;
        }
      }
      else
      {
        cpu->issueQueue[free_IQ] = cpu->dispatch;
      }
      // updating ROB for all instructions
      if (cpu->reorderBuffer->size != cpu->reorderBuffer->capacity)
      {
        cpu->reorderBuffer->rear = (cpu->reorderBuffer->rear + 1) % cpu->reorderBuffer->capacity;
        cpu->reorderBuffer->array[cpu->reorderBuffer->rear] = cpu->dispatch;
        cpu->reorderBuffer->size = cpu->reorderBuffer->size + 1;
        cpu->reorderBuffer->array[cpu->reorderBuffer->rear].lsq_index = cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].lsq_index;
      }
      if (ENABLE_DEBUG_MESSAGES)
      {
        print_stage_content("Rename2/Dispatch", &cpu->dispatch);
      }
    }
    else
    {
      if (ENABLE_DEBUG_MESSAGES)
      {
        if (cpu->dispatch.has_insn == FALSE)
        {
        }
        else
        {
          print_stage_content("Rename2/Dispatch", &cpu->dispatch);
        }
      }
    }
  }
  else
  {
    if (ENABLE_DEBUG_MESSAGES)
    {
      if (cpu->dispatch.has_insn == FALSE)
      {
        print_stage_empty("Rename2/Dispatch");
      }
      else
      {
        print_stage_content("Rename2/Dispatch", &cpu->dispatch);
      }
    }
  }
}
static void APEX_issueQueueUpdate(APEX_CPU *cpu)
{
  for (int i = 0; i < IQ_SIZE; i++)
  {
    // src1 updates
    if (cpu->issueQueue[i].src1_validbit == 1)
    {
      if (cpu->physicalregs[cpu->issueQueue[i].src1_tag].isValid == 0)
      {
        cpu->issueQueue[i].src1_value = cpu->physicalregs[cpu->issueQueue[i].src1_tag].value;
        cpu->issueQueue[i].src1_validbit = 0;
      }
    }
    if (cpu->issueQueue[i].src2_validbit == 1)
    {
      if (cpu->physicalregs[cpu->issueQueue[i].src2_tag].isValid == 0)
      {
        cpu->issueQueue[i].src2_value = cpu->physicalregs[cpu->issueQueue[i].src2_tag].value;
        cpu->issueQueue[i].src2_validbit = 0;
      }
    }
    if (cpu->issueQueue[i].src3_validbit == 1)
    {
      if (cpu->physicalregs[cpu->issueQueue[i].src3_tag].isValid == 0)
      {
        cpu->issueQueue[i].src3_value = cpu->physicalregs[cpu->issueQueue[i].src3_tag].value;
        cpu->issueQueue[i].src3_validbit = 0;
      }
    }
  }
  if (cpu->issueQueue[0].free_bit == 1)
  {

    for (int i = 0; i < IQ_SIZE; i++)
    {
      if (cpu->issueQueue[i].src1_validbit == 0 && cpu->issueQueue[i].src2_validbit == 0 && cpu->issueQueue[i].src3_validbit == 0)
      {

        if (cpu->issueQueue[i].free_bit == 1)
        {
          if (cpu->mulFU1.has_insn == 0 && strcmp(cpu->issueQueue[i].opcode_str, "MUL") == 0)
          {
            cpu->mulFU1.pc = cpu->issueQueue[i].pc;
            strcpy(cpu->mulFU1.opcode_str, cpu->issueQueue[i].opcode_str);
            cpu->mulFU1.opcode = cpu->issueQueue[i].opcode;
            cpu->mulFU1.rs1 = cpu->issueQueue[i].rs1;
            cpu->mulFU1.rs2 = cpu->issueQueue[i].rs2;
            cpu->mulFU1.rs3 = cpu->issueQueue[i].rs3;
            cpu->mulFU1.rd = cpu->issueQueue[i].rd;
            cpu->mulFU1.imm = cpu->issueQueue[i].imm;
            cpu->mulFU1.rs1_value = cpu->issueQueue[i].rs1_value;
            cpu->mulFU1.rs2_value = cpu->issueQueue[i].rs2_value;
            cpu->mulFU1.rs3_value = cpu->issueQueue[i].rs3_value;
            cpu->mulFU1.result_buffer = cpu->issueQueue[i].result_buffer;
            cpu->mulFU1.memory_address = cpu->issueQueue[i].memory_address;
            cpu->mulFU1.has_insn = cpu->issueQueue[i].has_insn;
            cpu->mulFU1.stalled = cpu->issueQueue[i].stalled;
            cpu->mulFU1.free_bit = cpu->issueQueue[i].free_bit;
            cpu->mulFU1.src1_validbit = cpu->issueQueue[i].src1_validbit;
            cpu->mulFU1.src1_tag = cpu->issueQueue[i].src1_tag;
            cpu->mulFU1.src1_value = cpu->issueQueue[i].src1_value;
            cpu->mulFU1.src2_validbit = cpu->issueQueue[i].src2_validbit;
            cpu->mulFU1.src2_tag = cpu->issueQueue[i].src2_tag;
            cpu->mulFU1.src2_value = cpu->issueQueue[i].src2_value;
            cpu->mulFU1.src3_validbit = cpu->issueQueue[i].src3_validbit;
            cpu->mulFU1.src3_tag = cpu->issueQueue[i].src3_tag;
            cpu->mulFU1.src3_value = cpu->issueQueue[i].src3_value;
            cpu->mulFU1.dtype = cpu->issueQueue[i].dtype;
            cpu->mulFU1.dest = cpu->issueQueue[i].dest;
            cpu->mulFU1.has_insn = 1;
            for (int j = i; j < IQ_SIZE; j++)
            {
              cpu->issueQueue[i] = cpu->issueQueue[j - 1];
            }
          }
          else if (cpu->logicalopFU.has_insn == 0 && (strcmp(cpu->issueQueue[i].opcode_str, "OR") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "EX-OR") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "AND") == 0) && cpu->issueQueue[i].src1_validbit == 0 && cpu->issueQueue[i].src2_validbit == 0)
          {
            cpu->logicalopFU.pc = cpu->issueQueue[i].pc;
            strcpy(cpu->logicalopFU.opcode_str, cpu->issueQueue[i].opcode_str);
            cpu->logicalopFU.opcode = cpu->issueQueue[i].opcode;
            cpu->logicalopFU.rs1 = cpu->issueQueue[i].rs1;
            cpu->logicalopFU.rs2 = cpu->issueQueue[i].rs2;
            cpu->logicalopFU.rs3 = cpu->issueQueue[i].rs3;
            cpu->logicalopFU.rd = cpu->issueQueue[i].rd;
            cpu->logicalopFU.imm = cpu->issueQueue[i].imm;
            cpu->logicalopFU.rs1_value = cpu->issueQueue[i].rs1_value;
            cpu->logicalopFU.rs2_value = cpu->issueQueue[i].rs2_value;
            cpu->logicalopFU.rs3_value = cpu->issueQueue[i].rs3_value;
            cpu->logicalopFU.result_buffer = cpu->issueQueue[i].result_buffer;
            cpu->logicalopFU.memory_address = cpu->issueQueue[i].memory_address;
            cpu->logicalopFU.has_insn = cpu->issueQueue[i].has_insn;
            cpu->logicalopFU.stalled = cpu->issueQueue[i].stalled;
            cpu->logicalopFU.free_bit = cpu->issueQueue[i].free_bit;
            cpu->logicalopFU.src1_validbit = cpu->issueQueue[i].src1_validbit;
            cpu->logicalopFU.src1_tag = cpu->issueQueue[i].src1_tag;
            cpu->logicalopFU.src1_value = cpu->issueQueue[i].src1_value;
            cpu->logicalopFU.src2_validbit = cpu->issueQueue[i].src2_validbit;
            cpu->logicalopFU.src2_tag = cpu->issueQueue[i].src2_tag;
            cpu->logicalopFU.src2_value = cpu->issueQueue[i].src2_value;
            cpu->logicalopFU.src3_validbit = cpu->issueQueue[i].src3_validbit;
            cpu->logicalopFU.src3_tag = cpu->issueQueue[i].src3_tag;
            cpu->logicalopFU.src3_value = cpu->issueQueue[i].src3_value;
            cpu->logicalopFU.dtype = cpu->issueQueue[i].dtype;
            cpu->logicalopFU.dest = cpu->issueQueue[i].dest;
            cpu->logicalopFU.has_insn = 1;

            for (int j = i; j < IQ_SIZE; j++)
            {
              cpu->issueQueue[i] = cpu->issueQueue[j - 1];
            }
          }
          else if (cpu->intFU.has_insn == 0 && (strcmp(cpu->issueQueue[i].opcode_str, "MOVC") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "ADD") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "ADDL") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "SUB") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "SUBL") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "DIV") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "JUMP") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "BZ") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "BNZ") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "CMP") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "NOP") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "HALT") == 0) && cpu->issueQueue[i].src1_validbit == 0 && cpu->issueQueue[i].src2_validbit == 0 && cpu->issueQueue[i].src3_validbit == 0)
          {
            cpu->intFU.pc = cpu->issueQueue[i].pc;
            strcpy(cpu->intFU.opcode_str, cpu->issueQueue[i].opcode_str);
            cpu->intFU.opcode = cpu->issueQueue[i].opcode;
            cpu->intFU.rs1 = cpu->issueQueue[i].rs1;
            cpu->intFU.rs2 = cpu->issueQueue[i].rs2;
            cpu->intFU.rs3 = cpu->issueQueue[i].rs3;
            cpu->intFU.rd = cpu->issueQueue[i].rd;
            cpu->intFU.imm = cpu->issueQueue[i].imm;
            cpu->intFU.rs1_value = cpu->issueQueue[i].rs1_value;
            cpu->intFU.rs2_value = cpu->issueQueue[i].rs2_value;
            cpu->intFU.rs3_value = cpu->issueQueue[i].rs3_value;
            cpu->intFU.result_buffer = cpu->issueQueue[i].result_buffer;
            cpu->intFU.memory_address = cpu->issueQueue[i].memory_address;
            cpu->intFU.has_insn = cpu->issueQueue[i].has_insn;
            cpu->intFU.stalled = cpu->issueQueue[i].stalled;
            cpu->intFU.free_bit = cpu->issueQueue[i].free_bit;
            cpu->intFU.src1_validbit = cpu->issueQueue[i].src1_validbit;
            cpu->intFU.src1_tag = cpu->issueQueue[i].src1_tag;
            cpu->intFU.src1_value = cpu->issueQueue[i].src1_value;
            cpu->intFU.src2_validbit = cpu->issueQueue[i].src2_validbit;
            cpu->intFU.src2_tag = cpu->issueQueue[i].src2_tag;
            cpu->intFU.src2_value = cpu->issueQueue[i].src2_value;
            cpu->intFU.src3_validbit = cpu->issueQueue[i].src3_validbit;
            cpu->intFU.src3_tag = cpu->issueQueue[i].src3_tag;
            cpu->intFU.src3_value = cpu->issueQueue[i].src3_value;
            cpu->intFU.dtype = cpu->issueQueue[i].dtype;
            cpu->intFU.dest = cpu->issueQueue[i].dest;
            cpu->intFU.has_insn = 1;
            if (strcmp(cpu->issueQueue[i].opcode_str, "JUMP") == 0)
            {
              cpu->dispatch.has_insn = TRUE;
            }

            for (int j = i; j < IQ_SIZE; j++)
            {
              cpu->issueQueue[i] = cpu->issueQueue[j - 1];
            }
          }
        }
      }
    }
  }
}
static void APEX_LSQueueUpdate(APEX_CPU *cpu)
{
  if (cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].opcode == cpu->reorderBuffer->array[cpu->reorderBuffer->front].opcode && cpu->loadStoreQueue->size != 0)
  {
    if (cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src1_validbit == 1)
    {
      if (cpu->physicalregs[cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src1_tag].isValid == 0)
      {

        cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src1_value = cpu->physicalregs[cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src1_tag].value;
        cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src1_validbit = 0;
      }
    }
    if (cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src2_validbit == 1)
    {
      if (cpu->physicalregs[cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src2_tag].isValid == 0)
      {
        cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src2_value = cpu->physicalregs[cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src2_tag].value;
        cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src2_validbit = 0;
      }
    }
    if (cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src3_validbit == 1)
    {
      if (cpu->physicalregs[cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src3_tag].isValid == 0)
      {
        cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src3_value = cpu->physicalregs[cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src3_tag].value;
        cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src3_validbit = 0;
      }
    }
    if (cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src1_validbit == 0 && cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src2_validbit == 0 && cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src3_validbit == 0)
    {
      switch (cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].opcode)
      {
      case OPCODE_LOAD:
      {
        cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].memory_address = cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src1_value + cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].imm;
        cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].result_buffer = cpu->data_memory[cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].memory_address];
        cpu->physicalregs[cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].dest].value = cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].result_buffer;
        cpu->physicalregs[cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].dest].isValid = 0;
        break;
      }
      case OPCODE_LDR:
      {
        cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].memory_address = cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src1_value + cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src2_value;
        cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].result_buffer = cpu->data_memory[cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].memory_address];
        cpu->physicalregs[cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].dest].value = cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].result_buffer;
        cpu->physicalregs[cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].dest].isValid = 0;

        break;
      }

      case OPCODE_STORE:
      {
        cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].result_buffer = cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src1_value;
        cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].memory_address = cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src2_value + cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].imm;
        cpu->data_memory[cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].memory_address] = cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].result_buffer;

        break;
      }
      case OPCODE_STR:
      {
        cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].result_buffer = cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src1_value;
        cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].memory_address = cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src2_value + cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].src3_value;
        cpu->data_memory[cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].memory_address] = cpu->loadStoreQueue->array[cpu->loadStoreQueue->front].result_buffer;
        break;
      }
      default:
      {
        break;
      }
      }
      // delete entry from lsq
      cpu->loadStoreQueue->size = cpu->loadStoreQueue->size - 1;
      for (int k = cpu->loadStoreQueue->front; k < cpu->loadStoreQueue->rear; k++)
      {
        cpu->loadStoreQueue->array[k] = cpu->loadStoreQueue->array[k + 1];
      }
      cpu->loadStoreQueue->rear = cpu->loadStoreQueue->rear - 1;
    }
  }
}

void printROB(APEX_CPU *cpu)
{
  if (cpu->reorderBuffer->size != 0)
  {
    printf("\n");
    printf("ROB contents are:\n");
    for (int i = cpu->reorderBuffer->front; i <= cpu->reorderBuffer->rear; i++)
    {
      switch (cpu->reorderBuffer->array[i].opcode)
      {
      case OPCODE_ADD:
      case OPCODE_SUB:
      case OPCODE_MUL:
      case OPCODE_DIV:
      case OPCODE_AND:
      case OPCODE_OR:
      case OPCODE_XOR:
      case OPCODE_LDR:
      {
        printf("%s,R%d,R%d,R%d\n", cpu->reorderBuffer->array[i].opcode_str, cpu->reorderBuffer->array[i].rd, cpu->reorderBuffer->array[i].rs1,
               cpu->reorderBuffer->array[i].rs2);
        break;
      }
      case OPCODE_ADDL:
      case OPCODE_SUBL:
      case OPCODE_LOAD:
      {
        printf("%s,R%d,R%d,#%d\n", cpu->reorderBuffer->array[i].opcode_str, cpu->reorderBuffer->array[i].rd, cpu->reorderBuffer->array[i].rs1, cpu->reorderBuffer->array[i].imm);
        break;
      }

      case OPCODE_MOVC:
      {
        printf("%s,R%d,#%d\n", cpu->reorderBuffer->array[i].opcode_str, cpu->reorderBuffer->array[i].rd, cpu->reorderBuffer->array[i].imm);
        break;
      }
      case OPCODE_JUMP:
      {
        printf("%s,R%d,#%d\n", cpu->reorderBuffer->array[i].opcode_str, cpu->reorderBuffer->array[i].rs1, cpu->reorderBuffer->array[i].imm);
        break;
      }
      case OPCODE_STORE:
      {
        printf("%s,R%d,R%d,#%d\n", cpu->reorderBuffer->array[i].opcode_str, cpu->reorderBuffer->array[i].rs1, cpu->reorderBuffer->array[i].rs2,
               cpu->reorderBuffer->array[i].imm);
        break;
      }

      case OPCODE_STR:
      {
        printf("%s,R%d,R%d,R%d\n", cpu->reorderBuffer->array[i].opcode_str, cpu->reorderBuffer->array[i].rs1, cpu->reorderBuffer->array[i].rs2,
               cpu->reorderBuffer->array[i].rs3);
        break;
      }

      case OPCODE_BZ:
      case OPCODE_BNZ:
      {
        printf("%s,#%d\n", cpu->reorderBuffer->array[i].opcode_str, cpu->reorderBuffer->array[i].imm);
        break;
      }

      case OPCODE_HALT:
      case OPCODE_NOP:
      {
        printf("%s\n", cpu->reorderBuffer->array[i].opcode_str);
        break;
      }

      case OPCODE_CMP:
      {
        printf("%s,R%d,R%d,R%d\n", cpu->reorderBuffer->array[i].opcode_str, cpu->reorderBuffer->array[i].rd, cpu->reorderBuffer->array[i].rs1, cpu->reorderBuffer->array[i].rs2);
        break;
      }
      }
    }
    printf("\n");
  }
}
void printBTB(APEX_CPU *cpu)
{
  if (cpu->branchBuffer->size != 0)
  {
    printf("\n");
    printf("BTB contents are:\n");
    for (int i = cpu->branchBuffer->front; i <= cpu->branchBuffer->rear; i++)
    {
      printf("%d,%s,%d\n", cpu->branchBuffer->array[i].pc, cpu->branchBuffer->array[i].opcode_str, cpu->branchBuffer->array[i].hit_or_miss);
    }
    printf("\n");
  }
}
void printFreePhysical(APEX_CPU *cpu)
{
  if (cpu->freePhysicalRegister->size != 0)
  {
    printf("\n");
    printf("Free registers are:\n");
    for (int i = cpu->freePhysicalRegister->front; i <= cpu->freePhysicalRegister->rear; i++)
    {
      printf("%d, ", cpu->freePhysicalRegister->array[i]);
    }
    printf("\n");
  }
}
void printLSQ(APEX_CPU *cpu)
{
  if (cpu->loadStoreQueue->size != 0)
  {
    printf("LSQ contents are:\n");
    for (int i = cpu->loadStoreQueue->front; i <= cpu->loadStoreQueue->rear; i++)
    {
      switch (cpu->loadStoreQueue->array[i].opcode)
      {
      case OPCODE_LDR:
      {
        printf("%d,%s,R%d,R%d,R%d\n", cpu->loadStoreQueue->array[i].pc, cpu->loadStoreQueue->array[i].opcode_str, cpu->loadStoreQueue->array[i].rd, cpu->loadStoreQueue->array[i].rs1,
               cpu->loadStoreQueue->array[i].rs2);
        break;
      }
      case OPCODE_LOAD:
      {
        printf("%d,%s,R%d,R%d,#%d\n", cpu->loadStoreQueue->array[i].pc, cpu->loadStoreQueue->array[i].opcode_str, cpu->loadStoreQueue->array[i].rd, cpu->loadStoreQueue->array[i].rs1, cpu->loadStoreQueue->array[i].imm);
        break;
      }
      case OPCODE_STORE:
      {
        printf("%d,%s,R%d,R%d,#%d\n", cpu->loadStoreQueue->array[i].pc, cpu->loadStoreQueue->array[i].opcode_str, cpu->loadStoreQueue->array[i].rs1, cpu->loadStoreQueue->array[i].rs2,
               cpu->loadStoreQueue->array[i].imm);
        break;
      }

      case OPCODE_STR:
      {
        printf("%d,%s,R%d,R%d,R%d\n", cpu->loadStoreQueue->array[i].pc, cpu->loadStoreQueue->array[i].opcode_str, cpu->loadStoreQueue->array[i].rs1, cpu->loadStoreQueue->array[i].rs2,
               cpu->loadStoreQueue->array[i].rs3);
        break;
      }
      }
    }
    printf("\n");
  }
}
/*
 * Execute Stages of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
static void APEX_intFU(APEX_CPU *cpu)
{

  int array1[3];
  if (cpu->intFU.has_insn && cpu->intFU.stalled == 0)
  {
    /* intFU logic based on instruction type */
    switch (cpu->intFU.opcode)
    {
    case OPCODE_ADD:
    {

      cpu->intFU.result_buffer = cpu->intFU.src1_value + cpu->intFU.src2_value;

      if (cpu->intFU.result_buffer == 0)
      {
        cpu->zero_flag = TRUE;
      }
      else
      {
        cpu->zero_flag = FALSE;
      }
      break;
    }

    case OPCODE_MOVC:
    {

      cpu->intFU.result_buffer = cpu->intFU.imm;
      /* Set the zero flag based on the result buffer */
      if (cpu->intFU.result_buffer == 0)
      {
        cpu->zero_flag = TRUE;
      }
      else
      {
        cpu->zero_flag = FALSE;
      }
      break;
    }
    case OPCODE_BZ:
    {

      if (cpu->rename_table[8][0] == TRUE)
      {
        int hm;
        for (int i = cpu->branchBuffer->front; i <= cpu->branchBuffer->rear; i++)
        {
          if (cpu->branchBuffer->array[i].pc == cpu->intFU.pc)
          {
            hm = cpu->branchBuffer->array[i].hit_or_miss;
          }
        }
        if (hm == 0)
        {
          cpu->pc = cpu->intFU.pc + cpu->intFU.imm;

          cpu->fetch_from_next_cycle = TRUE;

          cpu->dispatch.has_insn = FALSE;
          cpu->decode.has_insn = FALSE;
          cpu->fetch.has_insn = TRUE;

          // ROB entries deleted
          while (TRUE)
          {
            if (cpu->reorderBuffer->array[cpu->reorderBuffer->rear].pc == cpu->intFU.pc || cpu->reorderBuffer->size == 0)
            {
              break;
            }
            else if (cpu->reorderBuffer->array[cpu->reorderBuffer->rear].pc > cpu->intFU.pc)
            {
              // delete rob entry from rear
              cpu->reorderBuffer->rear = cpu->reorderBuffer->rear - 1;
              cpu->reorderBuffer->size = cpu->reorderBuffer->size - 1;
            }
            else
            {
              break;
            }
          }
          // IQ entries deleted
          for (int i = IQ_SIZE - 1; i >= 0; i--)
          {
            if (cpu->issueQueue[i].free_bit == 1)
            {
              if (cpu->issueQueue[i].pc == cpu->intFU.pc)
              {
                break;
              }
              else if (cpu->issueQueue[i].pc > cpu->intFU.pc)
              {
                // delete rob entry from rear
                cpu->issueQueue[i].free_bit = 0;
                switch (cpu->issueQueue[i].opcode)
                {
                case OPCODE_ADD:
                case OPCODE_MOVC:
                case OPCODE_ADDL:
                case OPCODE_SUB:
                case OPCODE_SUBL:
                case OPCODE_DIV:
                case OPCODE_MUL:
                case OPCODE_AND:
                case OPCODE_OR:
                case OPCODE_XOR:
                case OPCODE_CMP:
                {
                  assignRegister(cpu->freePhysicalRegister, cpu->issueQueue[i].dest);
                  cpu->rename_table[cpu->issueQueue[i].rd][0] = cpu->issueQueue[i].prev_dest;
                  cpu->physicalregs[cpu->issueQueue[i].dest].isValid = 0;
                  break;
                }
                default:
                {
                  break;
                }
                }
              }
              else
              {
                break;
              }
            }
          }
          // LSQ entries deleted
          while (TRUE)
          {
            if (cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].pc == cpu->intFU.pc || cpu->loadStoreQueue->size == 0)
            {
              break;
            }
            else if (cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].pc > cpu->intFU.pc)
            {
              switch (cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].opcode)
                {
                case OPCODE_LOAD:
                case OPCODE_LDR:
                {
                  assignRegister(cpu->freePhysicalRegister, cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].dest);
                  cpu->physicalregs[cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].dest].isValid = 0;
                  cpu->rename_table[cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].rd][0] = cpu->rename_table[cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].prev_dest][0];
                  
                  break;
                }
                default:
                {
                  break;
                }
                }
              // delete rob entry from rear
              cpu->loadStoreQueue->rear = cpu->loadStoreQueue->rear - 1;
              cpu->loadStoreQueue->size = cpu->loadStoreQueue->size - 1;
            }
            else
            {
              break;
            }
          }
          switch (cpu->dispatch.opcode)
          {
          case OPCODE_ADD:
          case OPCODE_MOVC:
          case OPCODE_ADDL:
          case OPCODE_SUB:
          case OPCODE_SUBL:
          case OPCODE_DIV:
          case OPCODE_MUL:
          case OPCODE_AND:
          case OPCODE_OR:
          case OPCODE_XOR:
          case OPCODE_LOAD:
          case OPCODE_LDR:
          case OPCODE_CMP:
          {
            assignRegister(cpu->freePhysicalRegister, cpu->dispatch.dest);
            cpu->rename_table[cpu->dispatch.rd][0] = cpu->dispatch.prev_dest;
            cpu->physicalregs[cpu->dispatch.dest].isValid = 0;
            break;
          }
          default:
          {
            break;
          }
          }
          switch (cpu->decode.opcode)
          {
          case OPCODE_ADD:
          case OPCODE_MOVC:
          case OPCODE_ADDL:
          case OPCODE_SUB:
          case OPCODE_SUBL:
          case OPCODE_DIV:
          case OPCODE_MUL:
          case OPCODE_AND:
          case OPCODE_OR:
          case OPCODE_XOR:
          case OPCODE_LOAD:
          case OPCODE_LDR:
          case OPCODE_CMP:
          {
            assignRegister(cpu->freePhysicalRegister, cpu->decode.dest);
            cpu->rename_table[cpu->decode.rd][0] = cpu->decode.prev_dest;
            cpu->physicalregs[cpu->decode.dest].isValid = 0;
            break;
          }
          default:
          {
            break;
          }
          }

          for (int i = cpu->branchBuffer->front; i <= cpu->branchBuffer->rear; i++)
          {
            if (cpu->branchBuffer->array[i].pc == cpu->intFU.pc)
            {
              cpu->branchBuffer->array[i].hit_or_miss = 1;
            }
          }
        }
      }
      else
      {
        for (int i = cpu->branchBuffer->front; i <= cpu->branchBuffer->rear; i++)
        {
          if (cpu->branchBuffer->array[i].pc == cpu->intFU.pc)
          {
            if (cpu->branchBuffer->array[i].hit_or_miss == 1)
            {
              cpu->pc = cpu->intFU.pc + 4;
              cpu->fetch_from_next_cycle = TRUE;

              cpu->dispatch.has_insn = FALSE;
              cpu->decode.has_insn = FALSE;
              cpu->fetch.has_insn = TRUE;
              cpu->branchBuffer->array[i].hit_or_miss = 0;
              // ROB entries deleted
              while (TRUE)
              {
                if (cpu->reorderBuffer->array[cpu->reorderBuffer->rear].pc == cpu->intFU.pc || cpu->reorderBuffer->size == 0)
                {
                  break;
                }
                else if (cpu->intFU.imm < 0 && cpu->reorderBuffer->array[cpu->reorderBuffer->rear].pc < cpu->intFU.pc)
                {
                  // delete rob entry from rear
                  cpu->reorderBuffer->rear = cpu->reorderBuffer->rear - 1;
                  cpu->reorderBuffer->size = cpu->reorderBuffer->size - 1;
                }
                else if (cpu->intFU.imm > 0 && cpu->reorderBuffer->array[cpu->reorderBuffer->rear].pc > cpu->intFU.pc)
                {
                  // delete rob entry from rear
                  cpu->reorderBuffer->rear = cpu->reorderBuffer->rear - 1;
                  cpu->reorderBuffer->size = cpu->reorderBuffer->size - 1;
                }
                else
                {
                  break;
                }
              }
              // IQ entries deleted
              for (int i = IQ_SIZE - 1; i >= 0; i--)
              {
                if (cpu->issueQueue[i].free_bit == 1)
                {
                  if (cpu->issueQueue[i].pc == cpu->intFU.pc)
                  {
                    break;
                  }
                  else if (cpu->intFU.imm < 0 && cpu->issueQueue[i].pc < cpu->intFU.pc)
                  {

                    cpu->issueQueue[i].free_bit = 0;
                   switch (cpu->issueQueue[i].opcode)
                {
                case OPCODE_ADD:
                case OPCODE_MOVC:
                case OPCODE_ADDL:
                case OPCODE_SUB:
                case OPCODE_SUBL:
                case OPCODE_DIV:
                case OPCODE_MUL:
                case OPCODE_AND:
                case OPCODE_OR:
                case OPCODE_XOR:
                case OPCODE_CMP:
                {
                  assignRegister(cpu->freePhysicalRegister, cpu->issueQueue[i].dest);
                  cpu->rename_table[cpu->issueQueue[i].rd][0] = cpu->issueQueue[i].prev_dest;
                  cpu->physicalregs[cpu->issueQueue[i].dest].isValid = 0;
                  break;
                }
                default:
                {
                  break;
                }
                }
                  }
                  else if (cpu->intFU.imm > 0 && cpu->issueQueue[i].pc > cpu->intFU.pc)
                  {
                    cpu->issueQueue[i].free_bit = 0;

                    switch (cpu->issueQueue[i].opcode)
                {
                case OPCODE_ADD:
                case OPCODE_MOVC:
                case OPCODE_ADDL:
                case OPCODE_SUB:
                case OPCODE_SUBL:
                case OPCODE_DIV:
                case OPCODE_MUL:
                case OPCODE_AND:
                case OPCODE_OR:
                case OPCODE_XOR:
                case OPCODE_CMP:
                {
                  assignRegister(cpu->freePhysicalRegister, cpu->issueQueue[i].dest);
                  cpu->rename_table[cpu->issueQueue[i].rd][0]= cpu->issueQueue[i].prev_dest;
                  cpu->physicalregs[cpu->issueQueue[i].dest].isValid = 0;
                  break;
                }
                default:
                {
                  break;
                }
                }
                  }
                  else
                  {
                    break;
                  }
                }
              }
              // LSQ entries deleted
              while (TRUE)
              {
                if (cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].pc == cpu->intFU.pc || cpu->loadStoreQueue->size == 0)
                {
                  break;
                }
                else if (cpu->intFU.imm < 0 && cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].pc < cpu->intFU.pc)
                {
                  switch (cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].opcode)
                {
                case OPCODE_LOAD:
                case OPCODE_LDR:
                {
                  assignRegister(cpu->freePhysicalRegister, cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].dest);
                  cpu->physicalregs[cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].dest].isValid = 0;
                  cpu->rename_table[cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].rd][0] = cpu->rename_table[cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].prev_dest][0];
                  
                  break;
                }
                default:
                {
                  break;
                }
                }
                  cpu->loadStoreQueue->rear = cpu->loadStoreQueue->rear - 1;
                  cpu->loadStoreQueue->size = cpu->loadStoreQueue->size - 1;
                }
                else if (cpu->intFU.imm > 0 && cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].pc > cpu->intFU.pc)
                {
                   switch (cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].opcode)
                {
                case OPCODE_LOAD:
                case OPCODE_LDR:
                {
                  assignRegister(cpu->freePhysicalRegister, cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].dest);
                  cpu->physicalregs[cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].dest].isValid = 0;
                  cpu->rename_table[cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].rd][0] = cpu->rename_table[cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].prev_dest][0];
                  
                  break;
                }
                default:
                {
                  break;
                }
                }
                cpu->loadStoreQueue->rear = cpu->loadStoreQueue->rear - 1;
                  cpu->loadStoreQueue->size = cpu->loadStoreQueue->size - 1;
                }
                else
                {
                  break;
                }
              }
              switch (cpu->dispatch.opcode)
              {
              case OPCODE_ADD:
              case OPCODE_MOVC:
              case OPCODE_ADDL:
              case OPCODE_SUB:
              case OPCODE_SUBL:
              case OPCODE_DIV:
              case OPCODE_MUL:
              case OPCODE_AND:
              case OPCODE_OR:
              case OPCODE_XOR:
              case OPCODE_LOAD:
              case OPCODE_LDR:
              case OPCODE_CMP:
              {
                assignRegister(cpu->freePhysicalRegister, cpu->dispatch.dest);
                cpu->rename_table[cpu->dispatch.rd][0] = cpu->dispatch.prev_dest;
                cpu->physicalregs[cpu->dispatch.dest].isValid = 0;
                break;
              }
              default:
              {
                break;
              }
              }
              switch (cpu->decode.opcode)
              {
              case OPCODE_ADD:
              case OPCODE_MOVC:
              case OPCODE_ADDL:
              case OPCODE_SUB:
              case OPCODE_SUBL:
              case OPCODE_DIV:
              case OPCODE_MUL:
              case OPCODE_AND:
              case OPCODE_OR:
              case OPCODE_XOR:
              case OPCODE_LOAD:
              case OPCODE_LDR:
              case OPCODE_CMP:
              {
                assignRegister(cpu->freePhysicalRegister, cpu->decode.dest);
                cpu->rename_table[cpu->decode.rd][0] = cpu->decode.prev_dest;
                cpu->physicalregs[cpu->decode.dest].isValid = 0;
                break;
              }
              default:
              {
                break;
              }
              }
            }
          }
        }
      }
      break;
    }
    case OPCODE_BNZ:
    {

      if (cpu->rename_table[8][0] == FALSE)
      {
        int hm;
        for (int i = cpu->branchBuffer->front; i <= cpu->branchBuffer->rear; i++)
        {
          if (cpu->branchBuffer->array[i].pc == cpu->intFU.pc)
          {
            hm = cpu->branchBuffer->array[i].hit_or_miss;
          }
        }
        if (hm == 0)
        {
          cpu->pc = cpu->intFU.pc + cpu->intFU.imm;

          cpu->fetch_from_next_cycle = TRUE;

          cpu->dispatch.has_insn = FALSE;
          cpu->decode.has_insn = FALSE;
          cpu->fetch.has_insn = TRUE;

          // ROB entries deleted
          while (TRUE)
          {
            if (cpu->reorderBuffer->array[cpu->reorderBuffer->rear].pc == cpu->intFU.pc || cpu->reorderBuffer->size == 0)
            {
              break;
            }
            else if (cpu->reorderBuffer->array[cpu->reorderBuffer->rear].pc > cpu->intFU.pc)
            {
              // delete rob entry from rear
              cpu->reorderBuffer->rear = cpu->reorderBuffer->rear - 1;
              cpu->reorderBuffer->size = cpu->reorderBuffer->size - 1;
            }
            else
            {
              break;
            }
          }
          // IQ entries deleted
          for (int i = IQ_SIZE - 1; i >= 0; i--)
          {
            if (cpu->issueQueue[i].free_bit == 1)
            {
              if (cpu->issueQueue[i].pc == cpu->intFU.pc)
              {
                break;
              }
              else if (cpu->issueQueue[i].pc > cpu->intFU.pc)
              {
                // delete rob entry from rear
                cpu->issueQueue[i].free_bit = 0;

                switch (cpu->issueQueue[i].opcode)
                {
                case OPCODE_ADD:
                case OPCODE_MOVC:
                case OPCODE_ADDL:
                case OPCODE_SUB:
                case OPCODE_SUBL:
                case OPCODE_DIV:
                case OPCODE_MUL:
                case OPCODE_AND:
                case OPCODE_OR:
                case OPCODE_XOR:
                case OPCODE_CMP:
                {
                  assignRegister(cpu->freePhysicalRegister, cpu->issueQueue[i].dest);
                  cpu->rename_table[cpu->issueQueue[i].rd][0] = cpu->issueQueue[i].prev_dest;
                  cpu->physicalregs[cpu->issueQueue[i].dest].isValid = 0;
                  break;
                }
                default:
                {
                  break;
                }
                }
              }
              else
              {
                break;
              }
            }
          }
          // LSQ entries deleted
          while (TRUE)
          {
            if (cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].pc == cpu->intFU.pc || cpu->loadStoreQueue->size == 0)
            {
              break;
            }
            else if (cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].pc > cpu->intFU.pc)
            {
              switch (cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].opcode)
                {
                case OPCODE_LOAD:
                case OPCODE_LDR:
                {
                  assignRegister(cpu->freePhysicalRegister, cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].dest);
                  cpu->physicalregs[cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].dest].isValid = 0;
                  cpu->rename_table[cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].rd][0] = cpu->rename_table[cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].prev_dest][0];
                  
                  break;
                }
                default:
                {
                  break;
                }
                } // delete lsq entry from rear
              cpu->loadStoreQueue->rear = cpu->loadStoreQueue->rear - 1;
              cpu->loadStoreQueue->size = cpu->loadStoreQueue->size - 1;
            }
            else
            {
              break;
            }
          }
          for (int i = cpu->branchBuffer->front; i <= cpu->branchBuffer->rear; i++)
          {
            if (cpu->branchBuffer->array[i].pc == cpu->intFU.pc)
            {
              cpu->branchBuffer->array[i].hit_or_miss = 1;
            }
          }
          switch (cpu->dispatch.opcode)
          {
          case OPCODE_ADD:
          case OPCODE_MOVC:
          case OPCODE_ADDL:
          case OPCODE_SUB:
          case OPCODE_SUBL:
          case OPCODE_DIV:
          case OPCODE_MUL:
          case OPCODE_AND:
          case OPCODE_OR:
          case OPCODE_XOR:
          case OPCODE_LOAD:
          case OPCODE_LDR:
          case OPCODE_CMP:
          {
            assignRegister(cpu->freePhysicalRegister, cpu->dispatch.dest);
            cpu->rename_table[cpu->dispatch.rd][0] = cpu->dispatch.prev_dest;
            cpu->physicalregs[cpu->dispatch.dest].isValid = 0;
            break;
          }
          default:
          {
            break;
          }
          }
          switch (cpu->decode.opcode)
          {
          case OPCODE_ADD:
          case OPCODE_MOVC:
          case OPCODE_ADDL:
          case OPCODE_SUB:
          case OPCODE_SUBL:
          case OPCODE_DIV:
          case OPCODE_MUL:
          case OPCODE_AND:
          case OPCODE_OR:
          case OPCODE_XOR:
          case OPCODE_LOAD:
          case OPCODE_LDR:
          case OPCODE_CMP:
          {
            assignRegister(cpu->freePhysicalRegister, cpu->decode.dest);
            cpu->rename_table[cpu->decode.rd][0] = cpu->decode.prev_dest;
            cpu->physicalregs[cpu->decode.dest].isValid = 0;
            break;
          }
          default:
          {
            break;
          }
          }
        }
      }
      else
      {
        for (int i = cpu->branchBuffer->front; i <= cpu->branchBuffer->rear; i++)
        {
          if (cpu->branchBuffer->array[i].pc == cpu->intFU.pc)
          {
            if (cpu->branchBuffer->array[i].hit_or_miss == 1)
            {
              cpu->pc = cpu->intFU.pc + 4;
              cpu->fetch_from_next_cycle = TRUE;

              cpu->dispatch.has_insn = FALSE;
              cpu->decode.has_insn = FALSE;
              cpu->fetch.has_insn = TRUE;
              cpu->branchBuffer->array[i].hit_or_miss = 0;
              // ROB entries deleted
              while (TRUE)
              {
                if (cpu->reorderBuffer->array[cpu->reorderBuffer->rear].pc == cpu->intFU.pc || cpu->reorderBuffer->size == 0)
                {
                  break;
                }
                else if (cpu->intFU.imm < 0 && cpu->reorderBuffer->array[cpu->reorderBuffer->rear].pc < cpu->intFU.pc)
                {
                  // delete rob entry from rear
                  cpu->reorderBuffer->rear = cpu->reorderBuffer->rear - 1;
                  cpu->reorderBuffer->size = cpu->reorderBuffer->size - 1;
                }
                else if (cpu->intFU.imm > 0 && cpu->reorderBuffer->array[cpu->reorderBuffer->rear].pc > cpu->intFU.pc)
                {
                  // delete rob entry from rear
                  cpu->reorderBuffer->rear = cpu->reorderBuffer->rear - 1;
                  cpu->reorderBuffer->size = cpu->reorderBuffer->size - 1;
                }
                else
                {
                  break;
                }
              }
              // IQ entries deleted
              for (int i = IQ_SIZE - 1; i >= 0; i--)
              {
                if (cpu->issueQueue[i].free_bit == 1)
                {
                  if (cpu->issueQueue[i].pc == cpu->intFU.pc)
                  {
                    break;
                  }
                  else if (cpu->intFU.imm < 0 && cpu->issueQueue[i].pc < cpu->intFU.pc)
                  {

                    cpu->issueQueue[i].free_bit = 0;
                   switch (cpu->issueQueue[i].opcode)
                {
                case OPCODE_ADD:
                case OPCODE_MOVC:
                case OPCODE_ADDL:
                case OPCODE_SUB:
                case OPCODE_SUBL:
                case OPCODE_DIV:
                case OPCODE_MUL:
                case OPCODE_AND:
                case OPCODE_OR:
                case OPCODE_XOR:
                case OPCODE_CMP:
                {
                  assignRegister(cpu->freePhysicalRegister, cpu->issueQueue[i].dest);
                  cpu->rename_table[cpu->issueQueue[i].rd][0] = cpu->issueQueue[i].prev_dest;
                  cpu->physicalregs[cpu->issueQueue[i].dest].isValid = 0;
                  break;
                }
                default:
                {
                  break;
                }
                }
                  }
                  else if (cpu->intFU.imm > 0 && cpu->issueQueue[i].pc > cpu->intFU.pc)
                  {
                    cpu->issueQueue[i].free_bit = 0;

                    switch (cpu->issueQueue[i].opcode)
                {
                case OPCODE_ADD:
                case OPCODE_MOVC:
                case OPCODE_ADDL:
                case OPCODE_SUB:
                case OPCODE_SUBL:
                case OPCODE_DIV:
                case OPCODE_MUL:
                case OPCODE_AND:
                case OPCODE_OR:
                case OPCODE_XOR:
                case OPCODE_CMP:
                {
                  assignRegister(cpu->freePhysicalRegister, cpu->issueQueue[i].dest);
                  cpu->rename_table[cpu->issueQueue[i].rd][0]= cpu->issueQueue[i].prev_dest;
                  cpu->physicalregs[cpu->issueQueue[i].dest].isValid = 0;
                  break;
                }
                default:
                {
                  break;
                }
                }
                  }
                  else
                  {
                    break;
                  }
                }
              }
              // LSQ entries deleted
              while (TRUE)
              {
                if (cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].pc == cpu->intFU.pc || cpu->loadStoreQueue->size == 0)
                {
                  break;
                }
                else if (cpu->intFU.imm < 0 && cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].pc < cpu->intFU.pc)
                {
                   switch (cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].opcode)
                {
                case OPCODE_LOAD:
                case OPCODE_LDR:
                {
                  assignRegister(cpu->freePhysicalRegister, cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].dest);
                  cpu->physicalregs[cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].dest].isValid = 0;
                  cpu->rename_table[cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].rd][0] = cpu->rename_table[cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].prev_dest][0];
                  
                  break;
                }
                default:
                {
                  break;
                }
                }
                cpu->loadStoreQueue->rear = cpu->loadStoreQueue->rear - 1;
                  cpu->loadStoreQueue->size = cpu->loadStoreQueue->size - 1;
                }
                else if (cpu->intFU.imm > 0 && cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].pc > cpu->intFU.pc)
                {
                   switch (cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].opcode)
                {
                case OPCODE_LOAD:
                case OPCODE_LDR:
                {
                  assignRegister(cpu->freePhysicalRegister, cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].dest);
                  cpu->physicalregs[cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].dest].isValid = 0;
                  cpu->rename_table[cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].rd][0] = cpu->rename_table[cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].prev_dest][0];
                  
                  break;
                }
                default:
                {
                  break;
                }
                }cpu->loadStoreQueue->rear = cpu->loadStoreQueue->rear - 1;
                  cpu->loadStoreQueue->size = cpu->loadStoreQueue->size - 1;
                }
                else
                {
                  break;
                }
              }
              switch (cpu->dispatch.opcode)
              {
              case OPCODE_ADD:
              case OPCODE_MOVC:
              case OPCODE_ADDL:
              case OPCODE_SUB:
              case OPCODE_SUBL:
              case OPCODE_DIV:
              case OPCODE_MUL:
              case OPCODE_AND:
              case OPCODE_OR:
              case OPCODE_XOR:
              case OPCODE_LOAD:
              case OPCODE_LDR:
              case OPCODE_CMP:
              {
                assignRegister(cpu->freePhysicalRegister, cpu->dispatch.dest);
                cpu->rename_table[cpu->dispatch.rd][0] = cpu->dispatch.prev_dest;
                cpu->physicalregs[cpu->dispatch.dest].isValid = 0;
                break;
              }
              default:
              {
                break;
              }
              }
              switch (cpu->decode.opcode)
              {
              case OPCODE_ADD:
              case OPCODE_MOVC:
              case OPCODE_ADDL:
              case OPCODE_SUB:
              case OPCODE_SUBL:
              case OPCODE_DIV:
              case OPCODE_MUL:
              case OPCODE_AND:
              case OPCODE_OR:
              case OPCODE_XOR:
              case OPCODE_LOAD:
              case OPCODE_LDR:
              case OPCODE_CMP:
              {
                assignRegister(cpu->freePhysicalRegister, cpu->decode.dest);
                cpu->rename_table[cpu->decode.rd][0] = cpu->decode.prev_dest;
                cpu->physicalregs[cpu->decode.dest].isValid = 0;
                break;
              }
              default:
              {
                break;
              }
              }
            }
          }
        }
      }
      break;
    }
    case OPCODE_JUMP:
    {
      cpu->pc = cpu->intFU.imm + cpu->intFU.src1_value;

      cpu->fetch_from_next_cycle = TRUE;

      cpu->dispatch.has_insn = FALSE;
      cpu->decode.has_insn = FALSE;
      cpu->fetch.has_insn = TRUE;

      // ROB entries deleted
      while (TRUE)
      {
        if (cpu->reorderBuffer->array[cpu->reorderBuffer->rear].pc == cpu->intFU.pc || cpu->reorderBuffer->size == 0)
        {
          break;
        }
        else if (cpu->reorderBuffer->array[cpu->reorderBuffer->rear].pc > cpu->intFU.pc)
        {
          // delete rob entry from rear
          cpu->reorderBuffer->rear = cpu->reorderBuffer->rear - 1;
          cpu->reorderBuffer->size = cpu->reorderBuffer->size - 1;
        }
        else
        {
          break;
        }
      }
      // IQ entries deleted
      for (int i = IQ_SIZE - 1; i >= 0; i--)
      {
        if (cpu->issueQueue[i].free_bit == 1)
        {
          if (cpu->issueQueue[i].pc == cpu->intFU.pc)
          {
            break;
          }
          else if (cpu->issueQueue[i].pc > cpu->intFU.pc)
          {
            // delete rob entry from rear
            cpu->issueQueue[i].free_bit = 0;
            switch (cpu->issueQueue[i].opcode)
                {
                case OPCODE_ADD:
                case OPCODE_MOVC:
                case OPCODE_ADDL:
                case OPCODE_SUB:
                case OPCODE_SUBL:
                case OPCODE_DIV:
                case OPCODE_MUL:
                case OPCODE_AND:
                case OPCODE_OR:
                case OPCODE_XOR:
                case OPCODE_CMP:
                {
                  assignRegister(cpu->freePhysicalRegister, cpu->issueQueue[i].dest);
                  cpu->rename_table[cpu->issueQueue[i].rd][0] = cpu->issueQueue[i].prev_dest;
                  cpu->physicalregs[cpu->issueQueue[i].dest].isValid = 0;
                  break;
                }
                default:
                {
                  break;
                }
                }
          }
          else
          {
            break;
          }
        }
      }
      // LSQ entries deleted
      while (TRUE)
      {
        if (cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].pc == cpu->intFU.pc || cpu->loadStoreQueue->size == 0)
        {
          break;
        }
        else if (cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].pc > cpu->intFU.pc)
        {
           switch (cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].opcode)
                {
                case OPCODE_LOAD:
                case OPCODE_LDR:
                {
                  assignRegister(cpu->freePhysicalRegister, cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].dest);
                  cpu->physicalregs[cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].dest].isValid = 0;
                  cpu->rename_table[cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].rd][0] = cpu->rename_table[cpu->loadStoreQueue->array[cpu->loadStoreQueue->rear].prev_dest][0];
                  
                  break;
                }
                default:
                {
                  break;
                }
                }
          // delete rob entry from rear
          cpu->loadStoreQueue->rear = cpu->loadStoreQueue->rear - 1;
          cpu->loadStoreQueue->size = cpu->loadStoreQueue->size - 1;
        }
        else
        {
          break;
        }
      }
      switch (cpu->dispatch.opcode)
      {
      case OPCODE_ADD:
      case OPCODE_MOVC:
      case OPCODE_ADDL:
      case OPCODE_SUB:
      case OPCODE_SUBL:
      case OPCODE_DIV:
      case OPCODE_MUL:
      case OPCODE_AND:
      case OPCODE_OR:
      case OPCODE_XOR:
      case OPCODE_LOAD:
      case OPCODE_LDR:
      case OPCODE_CMP:
      {
        assignRegister(cpu->freePhysicalRegister, cpu->dispatch.dest);
        cpu->rename_table[cpu->dispatch.rd][0] = cpu->dispatch.prev_dest;
        cpu->physicalregs[cpu->dispatch.dest].isValid = 0;
        break;
      }
      default:
      {
        break;
      }
      }
      switch (cpu->decode.opcode)
      {
      case OPCODE_ADD:
      case OPCODE_MOVC:
      case OPCODE_ADDL:
      case OPCODE_SUB:
      case OPCODE_SUBL:
      case OPCODE_DIV:
      case OPCODE_MUL:
      case OPCODE_AND:
      case OPCODE_OR:
      case OPCODE_XOR:
      case OPCODE_LOAD:
      case OPCODE_LDR:
      case OPCODE_CMP:
      {
        assignRegister(cpu->freePhysicalRegister, cpu->decode.dest);
        cpu->rename_table[cpu->decode.rd][0] = cpu->decode.prev_dest;
        cpu->physicalregs[cpu->decode.dest].isValid = 0;
        break;
      }
      default:
      {
        break;
      }
      }
      for (int i = cpu->branchBuffer->front; i <= cpu->branchBuffer->rear; i++)
      {
        if (cpu->branchBuffer->array[i].pc == cpu->intFU.pc)
        {
          cpu->branchBuffer->array[i].hit_or_miss = 1;
        }
      }
      break;
    }

    case OPCODE_ADDL:
    {
      cpu->intFU.result_buffer = cpu->intFU.src1_value + cpu->intFU.imm;

      /* Set the zero flag based on the result buffer */
      if (cpu->intFU.result_buffer == 0)
      {
        cpu->zero_flag = TRUE;
      }
      else
      {
        cpu->zero_flag = FALSE;
      }
      break;
    }

    case OPCODE_SUB:
    {

      cpu->intFU.result_buffer = cpu->intFU.src1_value - cpu->intFU.src2_value;

      /* Set the zero flag based on the result buffer */
      if (cpu->intFU.result_buffer == 0)
      {
        cpu->zero_flag = TRUE;
      }
      else
      {
        cpu->zero_flag = FALSE;
      }
      break;
    }

    case OPCODE_SUBL:
    {

      cpu->intFU.result_buffer = cpu->intFU.src1_value - cpu->intFU.imm;
      /* Set the zero flag based on the result buffer */
      if (cpu->intFU.result_buffer == 0)
      {
        cpu->zero_flag = TRUE;
      }
      else
      {
        cpu->zero_flag = FALSE;
      }
      break;
    }

    case OPCODE_DIV:
    {

      if (cpu->intFU.rs2_value != 0)
      {
        cpu->intFU.result_buffer = cpu->intFU.src1_value / cpu->intFU.src2_value;
      }
      else
      {
        fprintf(stderr, "As the dividend is zero returning value zero\n");
        cpu->intFU.result_buffer = 0;
      }

      /* Set the zero flag based on the result buffer */
      if (cpu->intFU.result_buffer == 0)
      {
        cpu->zero_flag = TRUE;
      }
      else
      {
        cpu->zero_flag = FALSE;
      }
      break;
    }

    case OPCODE_HALT:
    {
      /* HALT doesn't have register operands */
      cpu->dispatch.has_insn = 0;

      break;
    }

    case OPCODE_NOP:
    {
      /* NOP doesn't have register operands */
      break;
    }

    case OPCODE_CMP:
    {

      cpu->intFU.result_buffer = cpu->intFU.src1_value - cpu->intFU.src2_value;
      /* Set the zero flag based on the result buffer */
      if (cpu->intFU.result_buffer == 0)
      {
        cpu->zero_flag = TRUE;
      }
      else
      {
        cpu->zero_flag = FALSE;
      }
      break;
    }
    default:
    {
      break;
    }
    }

    if (cpu->intFU.opcode != OPCODE_HALT && cpu->intFU.opcode != OPCODE_NOP && cpu->intFU.opcode != OPCODE_JUMP && cpu->intFU.opcode != OPCODE_BZ && cpu->intFU.opcode != OPCODE_BNZ)
    {
      array1[0] = cpu->intFU.dest;
      array1[1] = cpu->intFU.result_buffer;
      array1[2] = cpu->zero_flag;

      add_Newdata(&cpu->head, array1);
    }
    cpu->intFU.has_insn = FALSE;

    if (ENABLE_DEBUG_MESSAGES)
    {
      print_fu_unit("intFU", &cpu->intFU);
    }
  }
  else
  {
    if (ENABLE_DEBUG_MESSAGES)
    {
      print_stage_empty("intFU");
    }
  }
}
static void APEX_mulFU1(APEX_CPU *cpu)
{

  if (cpu->mulFU1.has_insn && cpu->mulFU1.stalled == 0)
  {
    /* mulFU1 logic based on instruction type */

    switch (cpu->mulFU1.opcode)
    {
    case OPCODE_MUL:
    {
      cpu->mulFU1.result_buffer = cpu->mulFU1.src1_value * cpu->mulFU1.src2_value;
      /* Set the zero flag based on the result buffer */
      if (cpu->mulFU1.result_buffer == 0)
      {
        cpu->zero_flag = TRUE;
      }
      else
      {
        cpu->zero_flag = FALSE;
      }
      break;
    }
    default:
    {
      break;
    }
    }
    cpu->mulFU2 = cpu->mulFU1;
    cpu->mulFU1.has_insn = FALSE;

    if (ENABLE_DEBUG_MESSAGES)
    {
      print_fu_unit("mulFU1", &cpu->mulFU1);
    }
  }
  else
  {
    if (ENABLE_DEBUG_MESSAGES)
    {

      print_stage_empty("mulFU1");
    }
  }
}
static void APEX_mulFU2(APEX_CPU *cpu)
{

  if (cpu->mulFU2.has_insn && cpu->mulFU2.stalled == 0)
  {
    /* mulFU2 logic based on instruction type */

    switch (cpu->mulFU2.opcode)
    {
    case OPCODE_MUL:
    {
      cpu->mulFU2.result_buffer = cpu->mulFU2.src1_value * cpu->mulFU2.src2_value;
      /* Set the zero flag based on the result buffer */
      break;
    }
    default:
    {
      break;
    }
    }
    cpu->mulFU3 = cpu->mulFU2;
    cpu->mulFU2.has_insn = FALSE;

    if (ENABLE_DEBUG_MESSAGES)
    {
      print_fu_unit("mulFU2", &cpu->mulFU2);
    }
  }
  else
  {
    if (ENABLE_DEBUG_MESSAGES)
    {

      print_stage_empty("mulFU2");
    }
  }
}
static void APEX_mulFU3(APEX_CPU *cpu)
{

  if (cpu->mulFU3.has_insn && cpu->mulFU3.stalled == 0)
  {
    /* mulFU3 logic based on instruction type */

    switch (cpu->mulFU3.opcode)
    {
    case OPCODE_MUL:
    {
      cpu->mulFU3.result_buffer = cpu->mulFU3.src1_value * cpu->mulFU3.src2_value;
      /* Set the zero flag based on the result buffer */
      break;
    }
    default:
    {
      break;
    }
    }
    cpu->mulFU4 = cpu->mulFU3;
    cpu->mulFU3.has_insn = FALSE;

    if (ENABLE_DEBUG_MESSAGES)
    {
      print_fu_unit("mulFU3", &cpu->mulFU3);
    }
  }
  else
  {
    if (ENABLE_DEBUG_MESSAGES)
    {

      print_stage_empty("mulFU3");
    }
  }
}
static void APEX_mulFU4(APEX_CPU *cpu)
{

  if (cpu->mulFU4.has_insn && cpu->mulFU4.stalled == 0)
  {
    /* mulFU4 logic based on instruction type */

    switch (cpu->mulFU4.opcode)
    {
    case OPCODE_MUL:
    {
      cpu->mulFU4.result_buffer = cpu->mulFU4.src1_value * cpu->mulFU4.src2_value;
      /* Set the zero flag based on the result buffer */
      break;
    }
    default:
    {
      break;
    }
    }
    array2[0] = cpu->mulFU4.dest;
    array2[1] = cpu->mulFU4.result_buffer;
    array2[2] = cpu->zero_flag;
    add_Newdata(&cpu->head, array2);
    cpu->mulFU4.has_insn = FALSE;

    if (ENABLE_DEBUG_MESSAGES)
    {
      print_fu_unit("mulFU4", &cpu->mulFU4);
    }
  }
  else
  {
    if (ENABLE_DEBUG_MESSAGES)
    {

      print_stage_empty("mulFU4");
    }
  }
}
static void APEX_logicalopFU(APEX_CPU *cpu)
{
  int array3[3];
  if (cpu->logicalopFU.has_insn && cpu->logicalopFU.stalled == 0)
  {

    /* logicalopFU logic based on instruction type */

    switch (cpu->logicalopFU.opcode)
    {
    case OPCODE_AND:
    {
      cpu->logicalopFU.result_buffer = cpu->logicalopFU.src1_value & cpu->logicalopFU.src2_value;

      /* Set the zero flag based on the result buffer */
      if (cpu->logicalopFU.result_buffer == 0)
      {
        cpu->zero_flag = TRUE;
      }
      else
      {
        cpu->zero_flag = FALSE;
      }
      break;
    }

    case OPCODE_OR:
    {
      cpu->logicalopFU.result_buffer = cpu->logicalopFU.src1_value | cpu->logicalopFU.src2_value;
      /* Set the zero flag based on the result buffer */
      if (cpu->logicalopFU.result_buffer == 0)
      {
        cpu->zero_flag = TRUE;
      }
      else
      {
        cpu->zero_flag = FALSE;
      }
      break;
    }

    case OPCODE_XOR:
    {
      cpu->logicalopFU.result_buffer = cpu->logicalopFU.src1_value ^ cpu->logicalopFU.src2_value;
      /* Set the zero flag based on the result buffer */
      if (cpu->logicalopFU.result_buffer == 0)
      {
        cpu->zero_flag = TRUE;
      }
      else
      {
        cpu->zero_flag = FALSE;
      }
      break;
    }
    default:
    {
      break;
    }
    }
    array3[0] = cpu->logicalopFU.dest;
    array3[1] = cpu->logicalopFU.result_buffer;
    array3[2] = cpu->zero_flag;
    add_Newdata(&cpu->head, array3);
    cpu->logicalopFU.has_insn = FALSE;

    if (ENABLE_DEBUG_MESSAGES)
    {
      print_fu_unit("logicalopFU", &cpu->logicalopFU);
    }
  }
  else
  {
    if (ENABLE_DEBUG_MESSAGES)
    {
      print_stage_empty("logicalopFU");
    }
  }
}

void moveDatatobus(APEX_CPU *cpu)
{
  if (cpu->head != NULL)
  {
    if (cpu->bus0.busy == 0)
    {
      Forwarding_Bus_0(cpu);
    }
  }
  if (cpu->head != NULL)
  {
    if (cpu->bus1.busy == 0)
    {
      Forwarding_Bus_1(cpu);
    }
  }
}
// bus0
void Forwarding_Bus_0(APEX_CPU *cpu)
{
  // enable datapart foor next cycle and use next_dat_bus0
  cpu->bus0.tag_part = cpu->head->data_values[0];
  cpu->bus0.data_part = cpu->head->data_values[1];
  cpu->bus0.flag_part = cpu->head->data_values[2];
  // print_bus0(cpu);
  cpu->physicalregs[cpu->bus0.tag_part].value = cpu->bus0.data_part;
  cpu->physicalregs[cpu->bus0.tag_part].zeroFlag = cpu->bus0.flag_part;
  cpu->rename_table[8][0] = cpu->bus0.flag_part;
  cpu->physicalregs[cpu->bus0.tag_part].isValid = 0;

  // remove from the list
  int returnarray2[3];
  removeDataAtPosiiton(&cpu->head, returnarray2);
  cpu->bus0.busy = 0;
}
// bus1
void Forwarding_Bus_1(APEX_CPU *cpu)
{

  cpu->bus1.tag_part = cpu->head->data_values[0];
  cpu->bus1.data_part = cpu->head->data_values[1];
  cpu->bus1.flag_part = cpu->head->data_values[2];
  // print_bus1(cpu);
  cpu->physicalregs[cpu->bus1.tag_part].value = cpu->bus1.data_part;
  cpu->physicalregs[cpu->bus1.tag_part].zeroFlag = cpu->bus1.flag_part;
  cpu->physicalregs[cpu->bus1.tag_part].isValid = 0;
  cpu->rename_table[8][0] = cpu->bus0.flag_part;

  // remove from the list
  int returnarray[3];
  removeDataAtPosiiton(&cpu->head, returnarray);
  cpu->bus1.busy = 0;
}
/*
 * Memory Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
int commit_on_archs(APEX_CPU *cpu)
{
  if (cpu->physicalregs[cpu->reorderBuffer->array[cpu->reorderBuffer->front].dest].isValid == 0 && cpu->reorderBuffer->size != 0)
  {
    switch (cpu->reorderBuffer->array[cpu->reorderBuffer->front].opcode)
    {
    case OPCODE_ADD:
    case OPCODE_MOVC:
    case OPCODE_ADDL:
    case OPCODE_SUB:
    case OPCODE_SUBL:
    case OPCODE_DIV:
    case OPCODE_MUL:
    case OPCODE_AND:
    case OPCODE_OR:
    case OPCODE_XOR:
    case OPCODE_LOAD:
    case OPCODE_LDR:
    {
      cpu->regs[cpu->reorderBuffer->array[cpu->reorderBuffer->front].rd] = cpu->physicalregs[cpu->reorderBuffer->array[cpu->reorderBuffer->front].dest].value;
      cpu->physicalregs[cpu->reorderBuffer->array[cpu->reorderBuffer->front].dest].isValid=0;
      cpu->cc[0] = cpu->physicalregs[cpu->reorderBuffer->array[cpu->reorderBuffer->front].dest].zeroFlag;
      // free physical register
      assignRegister(cpu->freePhysicalRegister, cpu->reorderBuffer->array[cpu->reorderBuffer->front].dest);
      cpu->rename_table[cpu->reorderBuffer->array[cpu->reorderBuffer->front].rd][1]=0;
      break;
    }
    case OPCODE_BZ:
    case OPCODE_JUMP:
    case OPCODE_BNZ:
    case OPCODE_NOP:
    {
      break;
    }
    case OPCODE_CMP:
    {
      cpu->cc[0] = cpu->physicalregs[cpu->reorderBuffer->array[cpu->reorderBuffer->front].dest].zeroFlag;
      // free physical register
      assignRegister(cpu->freePhysicalRegister, cpu->reorderBuffer->array[cpu->reorderBuffer->front].dest);
      cpu->rename_table[cpu->reorderBuffer->array[cpu->reorderBuffer->front].rd][1]=0;
      cpu->physicalregs[cpu->reorderBuffer->array[cpu->reorderBuffer->front].dest].isValid=0;
      break;
    }
    case OPCODE_HALT:
    {
      printf("HALT COMMITED\n");
      return 1;
      break;
    }
    default:
    {
      break;
    }
    }

    // delete at the head of ROB

    cpu->reorderBuffer->size = cpu->reorderBuffer->size - 1;
    for (int k = cpu->reorderBuffer->front; k < cpu->reorderBuffer->rear; k++)
    {
      cpu->reorderBuffer->array[k] = cpu->reorderBuffer->array[k + 1];
    }
    cpu->reorderBuffer->rear = cpu->reorderBuffer->rear - 1;
    cpu->insn_completed++;
  }
  else if (cpu->reorderBuffer->size != 0)
  {
    switch (cpu->reorderBuffer->array[cpu->reorderBuffer->front].opcode)
    {
    case OPCODE_BZ:
    case OPCODE_JUMP:
    case OPCODE_BNZ:
    case OPCODE_NOP:
    case OPCODE_STORE:
    case OPCODE_STR:
    {
      cpu->reorderBuffer->size = cpu->reorderBuffer->size - 1;
      for (int k = cpu->reorderBuffer->front; k < cpu->reorderBuffer->rear; k++)
      {
        cpu->reorderBuffer->array[k] = cpu->reorderBuffer->array[k + 1];
      }
      cpu->reorderBuffer->rear = cpu->reorderBuffer->rear - 1;
      cpu->insn_completed++;
      break;
    }
    default:
    {
      break;
    }
    }
  }
  return 0;
}
APEX_CPU *APEX_cpu_init(const char *filename)
{
  int i;
  APEX_CPU *cpu;

  if (!filename)
  {
    return NULL;
  }

  cpu = calloc(1, sizeof(APEX_CPU));

  if (!cpu)
  {
    return NULL;
  }

  /* Initialize PC, Registers and all pipeline stages */
  cpu->pc = 4000;
  memset(cpu->regs, 0, sizeof(int) * REG_FILE_SIZE);
  memset(cpu->data_memory, 0, sizeof(int) * DATA_MEMORY_SIZE);
  cpu->single_step = ENABLE_SINGLE_STEP;
  cpu->fetch.stalled = 0;
  cpu->decode.stalled = 0;
  cpu->intFU.stalled = 0;
  cpu->mulFU1.stalled = 0;
  cpu->mulFU2.stalled = 0;
  cpu->mulFU3.stalled = 0;
  cpu->mulFU4.stalled = 0;
  cpu->logicalopFU.stalled = 0;
  /* Parse input file and create code memory */
  cpu->code_memory = create_code_memory(filename, &cpu->code_memory_size);
  if (!cpu->code_memory)
  {
    free(cpu);
    return NULL;
  }
  cpu->freePhysicalRegister = establishQueueByCapacity(PREG_FILE_SIZE);
  cpu->reorderBuffer = establishROBQueueByCapacity(ROB_SIZE);
  cpu->loadStoreQueue = establishLSQQueueByCapacity(LSQ_SIZE);
  cpu->branchBuffer = establishBTBQueueByCapacity(BTB_SIZE);
  for (int i = 0; i < 15; i++)
  {
    assignRegister(cpu->freePhysicalRegister, i);
  }
  int k = 0;
  for (k = 0; k < REG_FILE_SIZE; k++)
  {
    cpu->rename_table[k][0] = -1;
  }
  cpu->rename_table[k][0] = cpu->cc[0];
  memset(cpu->issueQueue, 0, sizeof(APEX_Instruction) * IQ_SIZE);
  if (ENABLE_DEBUG_MESSAGES)
  {
    fprintf(stderr,
            "APEX_CPU: Initialized APEX CPU, loaded %d instructions\n",
            cpu->code_memory_size);
    fprintf(stderr, "APEX_CPU: PC initialized to %d\n", cpu->pc);
    fprintf(stderr, "APEX_CPU: Printing Code Memory\n");
    printf("%-9s %-9s %-9s %-9s %-9s\n", "opcode_str", "rd", "rs1", "rs2",
           "imm");

    for (i = 0; i < cpu->code_memory_size; ++i)
    {
      printf("%-9s %-9d %-9d %-9d %-9d\n", cpu->code_memory[i].opcode_str,
             cpu->code_memory[i].rd, cpu->code_memory[i].rs1,
             cpu->code_memory[i].rs2, cpu->code_memory[i].imm);
    }
  }
  /* To start fetch stage */
  cpu->fetch.has_insn = TRUE;
  return cpu;
}

/*
 * APEX CPU simulation loop
 *
 * Note: You are free to edit this function according to your implementation
 */
void APEX_cpu_run(APEX_CPU *cpu, const char *mode, int cycles)
{
  char user_prompt_val;
  int Non_stop = 1;
  while (TRUE)
  {
    if (strcmp(mode, "simulate") == 0 || strcmp(mode, "display") == 0)
    {
      Non_stop = 0;
      if (cycles == 0)
      {
        break;
      }
      else
      {
        cycles = cycles - 1;
      }
    }
    if (ENABLE_DEBUG_MESSAGES)
    {
      printf("--------------------------------------------\n");
      printf("Clock Cycle #: %d\n", cpu->clock + 1);
      printf("--------------------------------------------\n");
    }
    //print_physical_register_state(cpu);
    //print_rename_table(cpu);
    // Commit ROB & update LSQ
    if (commit_on_archs(cpu))
    {
      break;
    }
    APEX_LSQueueUpdate(cpu);
    //printFreePhysical(cpu);
    //printROB(cpu);
    //printLSQ(cpu);

    // Function units implemented
    APEX_mulFU4(cpu);
    APEX_mulFU3(cpu);
    APEX_mulFU2(cpu);
    APEX_mulFU1(cpu);
    APEX_logicalopFU(cpu);

    APEX_intFU(cpu);
   // Data transferred through forwarding bus
    moveDatatobus(cpu);

    // update issue Queue and transfer instructions to FU's
    //display_issueQueue(cpu);
    APEX_issueQueueUpdate(cpu);

    // Dispatch, Decode, Fetch
    // printf("dest (tag) value is %d, src1 tag is %d,src1 value is %d , src2 tag is %d and  src2 value is %d\n",cpu->dispatch.dest,cpu->dispatch.src1_value,cpu->dispatch.src1_tag,cpu->dispatch.src2_tag,cpu->dispatch.src2_value);

    APEX_dispatch(cpu);
    //printf("dest (tag) value is %d, src1 tag is %d,src1 value is %d, src1 valid bit is %d ,src2 valid bit is %d ,src2 tag is %d and  src2 value is %d\n",cpu->dispatch.dest,cpu->dispatch.src1_tag,cpu->dispatch.src1_value,cpu->dispatch.src1_validbit,cpu->dispatch.src2_validbit,cpu->dispatch.src2_tag,cpu->dispatch.src2_value);

    //printf("dest (tag) value is %d\n", cpu->dispatch.dest);
    // printf("dest (tag) value is %d, src1 tag is %d,src1 value is %d , src2 tag is %d and  src2 value is %d\n",cpu->decode.dest,cpu->decode.src1_value,cpu->decode.src1_tag,cpu->decode.src2_tag,cpu->decode.src2_value);

    APEX_decode(cpu);
    //printf("dest (tag) value is %d, src1 tag is %d,src1 value is %d, src1 valid bit is %d ,src2 valid bit is %d , src2 tag is %d and  src2 value is %d\n",cpu->decode.dest,cpu->decode.src1_tag,cpu->decode.src1_value,cpu->decode.src1_validbit,cpu->decode.src2_validbit,cpu->decode.src2_tag,cpu->decode.src2_value);

    //printf("dest (tag) value is %d\n", cpu->decode.dest);
    APEX_fetch(cpu);
    //printBTB(cpu);
    // commentbelow
    //print_reg_file(cpu);
    //printf("\nCC Flag contents: CC = %d\n\n", cpu->rename_table[8][0]);
    //print_data_memory(cpu);

    if (cpu->single_step && Non_stop)
    {
      printf("Press any key to next CPU clock or <q> to quit:\n");
      scanf("%c", &user_prompt_val);

      if ((user_prompt_val == 'Q') || (user_prompt_val == 'q'))
      {
        break;
      }
    }

    cpu->clock++;
  }
  printf("\n");
  printf("APEX_CPU: Simulation Complete, cycles = %d instructions = %d\n", cpu->clock+1, cpu->insn_completed);
  
  // print_physical_register_state(cpu);
  // print_rename_table(cpu);
  print_reg_file(cpu);
  print_data_memory(cpu);
  if (strcmp(mode, "display") == 0)
  {
    printROB(cpu);
    printLSQ(cpu);
    display_issueQueue(cpu);
    printBTB(cpu);
  }
}
/*
 * This function deallocates APEX CPU.
 *
 * Note: You are free to edit this function according to your implementation
 */
void APEX_cpu_stop(APEX_CPU *cpu)
{
  free(cpu->code_memory);
  free(cpu);
}

void assignRegister(struct PhyRegfile *queue, int item)
{
  if (queue->size == queue->capacity)
  {
    return;
  }

  queue->rear = (queue->rear + 1) % queue->capacity;
  // queue->rear = queue->rear + 1;
  queue->array[queue->rear] = item;
  queue->size = queue->size + 1;
}

int retreiveRegister(struct PhyRegfile *queue)
{
  if (queue->size == 0)
    return -1;
  int item = queue->array[queue->front];

  // queue->front = (queue->front + 1) % queue->capacity;
  // queue->size = queue->size - 1;
  queue->size = queue->size - 1;
  for (int k = queue->front; k < queue->rear; k++)
  {
    queue->array[k] = queue->array[k + 1];
  }
  queue->rear = queue->rear - 1;
  return item;
}

// need this?
/*
 * Issue Queue
 * */
int isIssueQueuecompletelyFull(APEX_CPU *cpu)
{
  for (int i = 0; i < IQ_SIZE; i++)
  {
    if (cpu->issueQueue[i].free_bit == 0)
    {
      return 0;
    }
  }
  return 1;
}
int isIssueQueueEmpty(APEX_CPU *cpu)
{
  if (cpu->issueQueue[0].free_bit == 0)
  {
    return 1;
  }
  return 0;
}