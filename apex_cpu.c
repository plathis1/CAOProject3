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
struct ReorderBuffer *createROBQueue(unsigned capacity)
{
  struct ReorderBuffer *rob = (struct ReorderBuffer *)malloc(sizeof(struct ReorderBuffer));
  rob->capacity = capacity;
  rob->head = rob->size = 0;
  rob->tail = capacity - 1;
  rob->array = (CPU_Stage *)malloc(rob->capacity * sizeof(CPU_Stage));
  return rob;
}
struct LSQ* createLSQQueue(unsigned capacity){
    struct LSQ* lsq = (struct LSQ*) malloc(sizeof(struct LSQ));
    lsq->capacity = capacity;
    lsq->front = lsq->size = 0;
    lsq->rear = capacity - 1;
    lsq->array = (CPU_Stage*) malloc(lsq->capacity * sizeof(CPU_Stage));
    return lsq;
}
// list
void add_at_rear(struct Node **head_ref, int new_data[])
{
  struct Node *new_node = (struct Node *)malloc(sizeof(struct Node));
  struct Node *last = *head_ref;
  new_node->data_values[0] = new_data[0];
  new_node->data_values[1] = new_data[1];
  new_node->next = NULL;
  if (*head_ref == NULL)
  {
    *head_ref = new_node;
    return;
  }
  while (last->next != NULL)
    last = last->next;

  last->next = new_node;
  return;
}
int *remove_at_front(struct Node **head_ref, int *returnarray)
{

  struct Node *temp = *head_ref;

  if (temp != NULL)
  {
    returnarray[0] = (*head_ref)->data_values[0];
    returnarray[1] = (*head_ref)->data_values[1];
    *head_ref = temp->next;
    free(temp);
    return returnarray;
  }
  if (temp == NULL)
    return returnarray;

  return returnarray;
  free(temp);
}
void printList(struct Node *node)
{
  while (node != NULL)
  {
    printf(" %d ", node->data_values[0]);
    printf(" %d \n", node->data_values[1]);
    node = node->next;
  }
}
int isEmpty(struct Node *node)
{
  if (node != NULL)
  {
    //printf("list is not empty\n");
    return 0;
  }
  //printf("list is empty\n");
  return 1;
}
void print_bus0(APEX_CPU *cpu)
{ // print and iterate through the list
  printf("Bus 0 have values %d and %d\n", cpu->bus0.tag_part, cpu->bus0.data_part);
}
void print_bus1(APEX_CPU *cpu)
{ // print and iterate through the list
  printf("Bus 1 have values %d and %d\n", cpu->bus1.tag_part, cpu->bus1.data_part);
}
/* Converts the PC(4000 series) into array index for code memory
 *
 * Note: You are not supposed to edit this function
 */
struct PhysicalRegistersQueue *createQueue(unsigned capacity)
{

  struct PhysicalRegistersQueue *queue = (struct PhysicalRegistersQueue *)malloc(sizeof(struct PhysicalRegistersQueue));
  queue->capacity = capacity;
  queue->front = queue->size = 0;
  queue->rear = capacity - 1;
  queue->array = (int *)malloc(queue->capacity * sizeof(int));
  return queue;
}
int print_physical_register_state(APEX_CPU *cpu)
{
  printf("\n=============== STATE OF PHYSICAL ARCHITECTURAL REGISTER FILE ==========\n");
  int count = 0;
  while (count < PREG_FILE_SIZE)
  {
    printf("|  P_REG[%d]  | V-bit = %s  | Value = %d  | CC = %d  \n", count, (!cpu->physicalregs[count].isValid ? "VALID" : "INVALID"), cpu->physicalregs[count].value, cpu->physicalregs[count].zeroFlag);
    count++;
  }
  return 0;
}
int print_rename_table(APEX_CPU *cpu)
{
  for (int i = 0; i < 9; i++)
  {
    if (cpu->rename_table[i] != -1)
    {
      printf("RAT[%d] --> P%d ---->and value=%d\n", i, cpu->rename_table[i], cpu->physicalregs[cpu->rename_table[i]].value);
    }
  }
  return 0;
}
static int get_code_memory_index_from_pc(const int pc)
{
  return (pc - 4000) / 4;
}
static void printContentsOfFreeRegistersList(APEX_CPU *cpu)
{
  printf("\n\n\n|\tFree Registers\t|\n\n\n");
  printf("Head ->\t");
  for (int i = cpu->freePhysicalRegister->front; i <= cpu->freePhysicalRegister->rear; i++)
  {
    printf("%d \t", cpu->freePhysicalRegister->array[i]);
  }
  printf("\n");
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
    printf("%s,R%d,R%d ", stage->opcode_str, stage->rs1, stage->rs2);
    break;
  }
  }
}
static void print_fun_unit(const char *name, const CPU_Stage *stage)
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
    printf("%s,P%d,P%d ", stage->opcode_str, stage->src1_tag, stage->src2_tag);
    break;
  }
  }
  printf("\n");
}
static void print_issueQueue(APEX_CPU *cpu)
{
  printf("IQ contents are:\n");
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
        printf("%s,P%d,P%d ", cpu->issueQueue[i].opcode_str, cpu->issueQueue[i].rs1, cpu->issueQueue[i].rs2);
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
  int no_registers = 16;
  while (no_of_ins < no_registers)
  {
    printf("| \t REG[%d] \t | \t Value = %d \t | \t Status = %s \t \n", no_of_ins, cpu->regs[no_of_ins], (!cpu->valid_regs[no_of_ins] ? "VALID" : "INVALID"));
    no_of_ins++;
  }
  return 0;
}

int print_data_memory(APEX_CPU *cpu)
{
  printf("\n=============== STATE OF DATA MEMORY =============\n");
  printf("\n");
  int no_of_ins = 0;
  int count = 0;
  while (no_of_ins < 100)
  {
    if (cpu->data_memory[no_of_ins] != 0)
    {
      printf("MEM[%d]  = %d\n", no_of_ins, cpu->data_memory[no_of_ins]);
      count++;
    }
    no_of_ins++;
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

  printf("----------\n%s\n----------\n", "Registers:");

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

    if (cpu->decode.stalled == 0)
    {
      /* Update PC for next instruction */
      cpu->pc += 4;

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
  if (isRegisterQueueEmpty(cpu->freePhysicalRegister) == 1)
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
      int free = getRegister(cpu->freePhysicalRegister);

      // step2 lookup the rename table for most recent instances of source registers
      // src1
      cpu->decode.src1_validbit = cpu->physicalregs[cpu->rename_table[cpu->decode.rs1]].isValid;
      cpu->decode.src1_tag = cpu->rename_table[cpu->decode.rs1];
      cpu->decode.src1_value = cpu->physicalregs[cpu->rename_table[cpu->decode.rs1]].value;
      // src2
      cpu->decode.src2_validbit = cpu->physicalregs[cpu->rename_table[cpu->decode.rs2]].isValid;
      cpu->decode.src2_tag = cpu->rename_table[cpu->decode.rs2];
      cpu->decode.src2_value = cpu->physicalregs[cpu->rename_table[cpu->decode.rs1]].value;

      // step 3 update latest instance of rd in rename table
      cpu->rename_table[cpu->decode.rd] = free;
      cpu->physicalregs[free].isValid = 1;
      cpu->allocationList[free] = 1;

      break;
    }
    case OPCODE_STORE:
    {

      // step2 lookup the rename table for most recent instances of source registers
      // src1
      cpu->decode.src1_validbit = cpu->physicalregs[cpu->rename_table[cpu->decode.rs1]].isValid;
      cpu->decode.src1_tag = cpu->rename_table[cpu->decode.rs1];
      cpu->decode.src1_value = cpu->physicalregs[cpu->rename_table[cpu->decode.rs1]].value;
      // src2
      cpu->decode.src2_validbit = cpu->physicalregs[cpu->rename_table[cpu->decode.rs2]].isValid;
      cpu->decode.src2_tag = cpu->rename_table[cpu->decode.rs2];
      cpu->decode.src2_value = cpu->physicalregs[cpu->rename_table[cpu->decode.rs1]].value;

      break;
    }
    case OPCODE_MOVC:
    {
      int free = getRegister(cpu->freePhysicalRegister);

      cpu->rename_table[cpu->decode.rd] = free;
      cpu->allocationList[free] = 1;
      cpu->physicalregs[free].isValid = 1;
      /* MOVC doesn't have register operands */
      break;
    }
    case OPCODE_JUMP:
    {
      // step 2
      // src1
      cpu->decode.src1_validbit = cpu->physicalregs[cpu->rename_table[cpu->decode.rs1]].isValid;
      cpu->decode.src1_tag = cpu->rename_table[cpu->decode.rs1];
      cpu->decode.src1_value = cpu->physicalregs[cpu->rename_table[cpu->decode.rs1]].value;

      break;
    }

    case OPCODE_ADDL:
    case OPCODE_SUBL:
    case OPCODE_LOAD:
    {
      // step 1
      int free = getRegister(cpu->freePhysicalRegister);
      // step 2
      // src1
      cpu->decode.src1_validbit = cpu->physicalregs[cpu->rename_table[cpu->decode.rs1]].isValid;
      cpu->decode.src1_tag = cpu->rename_table[cpu->decode.rs1];
      cpu->decode.src1_value = cpu->physicalregs[cpu->rename_table[cpu->decode.rs1]].value;
      // step 3
      cpu->rename_table[cpu->decode.rd] = free;
      cpu->allocationList[free] = 1;
      cpu->physicalregs[free].isValid = 1;

      break;
    }

    case OPCODE_STR:
    {

      // src1
      cpu->decode.src1_validbit = cpu->physicalregs[cpu->rename_table[cpu->decode.rs1]].isValid;
      cpu->decode.src1_tag = cpu->rename_table[cpu->decode.rs1];
      cpu->decode.src1_value = cpu->physicalregs[cpu->rename_table[cpu->decode.rs1]].value;
      // src2
      cpu->decode.src2_validbit = cpu->physicalregs[cpu->rename_table[cpu->decode.rs2]].isValid;
      cpu->decode.src2_tag = cpu->rename_table[cpu->decode.rs2];
      cpu->decode.src2_value = cpu->physicalregs[cpu->rename_table[cpu->decode.rs1]].value;
      // src3
      cpu->decode.src3_validbit = cpu->physicalregs[cpu->rename_table[cpu->decode.rs3]].isValid;
      cpu->decode.src3_tag = cpu->rename_table[cpu->decode.rs3];
      cpu->decode.src3_value = cpu->physicalregs[cpu->rename_table[cpu->decode.rs3]].value;

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
      if (!(cpu->dispatch.stalled || isIssueQueueFull(cpu)))
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
  if (cpu->dispatch.has_insn && cpu->dispatch.stalled == 0 && isIssueQueueFull(cpu) == 0 && isROBFull(cpu->reorderBuffer) == 0 && isLSQFull(cpu->loadStoreQueue)==0){
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
      cpu->dispatch.src3_validbit = 1; // not using r3 so made it valid
      cpu->dispatch.src3_tag = 0;
      cpu->dispatch.dtype = 1;
      cpu->dispatch.dest = cpu->rename_table[cpu->dispatch.rd];
      break;
    }
    case OPCODE_MUL:
    {
      // prepare IQ if an entry is available
      cpu->dispatch.free_bit = 1;
      cpu->dispatch.src3_validbit = 1; // not using r3 so made it valid
      cpu->dispatch.src3_tag = 0;
      cpu->dispatch.dtype = 1;
      cpu->dispatch.dest = cpu->rename_table[cpu->dispatch.rd];
      break;
    }
    case OPCODE_AND:
    case OPCODE_OR:
    case OPCODE_XOR:
    {

      // prepare IQ if an entry is available
      cpu->dispatch.free_bit = 1;
      cpu->dispatch.src3_validbit = 1; // not using r3 so made it valid
      cpu->dispatch.src3_tag = 0;
      cpu->dispatch.dtype = 1;
      cpu->dispatch.dest = cpu->rename_table[cpu->dispatch.rd];
      break;
    }

    case OPCODE_MOVC:
    {
      /* MOVC doesn't have register operands */
      // prepare IQ if an entry is available
      cpu->dispatch.free_bit = 1;
      cpu->dispatch.src1_validbit = 1;
      cpu->dispatch.src1_tag = 0;
      cpu->dispatch.src2_validbit = 1;
      cpu->dispatch.src1_tag = 0;
      cpu->dispatch.src3_validbit = 1;
      cpu->dispatch.src3_tag = 0;
      cpu->dispatch.dtype = 1;
      cpu->dispatch.dest = cpu->rename_table[cpu->dispatch.rd];
      break;
    }
    case OPCODE_JUMP:
    {
      // prepare IQ if an entry is available
      cpu->dispatch.free_bit = 1;
      cpu->dispatch.src2_validbit = 1; // initialized to valid
      cpu->dispatch.src1_tag = 0;
      cpu->dispatch.src3_validbit = 1; // not using r3 so made it valid
      cpu->dispatch.src3_tag = 0;
      cpu->dispatch.dtype = 1;
      cpu->dispatch.dest = cpu->rename_table[cpu->dispatch.rd];
      break;
    }

    case OPCODE_ADDL:
    case OPCODE_SUBL:
    case OPCODE_LOAD:
    {
      // prepare IQ if an entry is available
      cpu->dispatch.free_bit = 1;
      cpu->dispatch.src2_validbit = 1; // not using r2 so make it valid
      cpu->dispatch.src2_tag = 0;
      cpu->dispatch.src3_validbit = 1; // not using r3 so made it valid
      cpu->dispatch.src3_tag = 0;
      cpu->dispatch.dtype = 1;
      cpu->dispatch.dest = cpu->rename_table[cpu->dispatch.rd];
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
      break;
    }

    case OPCODE_BNZ:
    {
      /* BNZ doesn't have register operands */
      break;
    }

    case OPCODE_HALT:
    {
      /* HALT doesn't have register operands */
      cpu->decode.has_insn = 0;
      break;
    }

    case OPCODE_NOP:
    {
      /* NOP doesn't have register operands */
      break;
    }

    default:
    {
      break;
    }
    }

    if (cpu->dispatch.stalled == 0 && isIssueQueueFull(cpu) == 0 && isROBFull(cpu->reorderBuffer) == 0)
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
      cpu->issueQueue[free_IQ] = cpu->dispatch;
      UpdateROB(cpu);
      // pushToIssueQueue(cpu,cpu->dispatch);
      // pushToROB(cpu->reorderBuffer,cpu->dispatch);
      // pushToLSQ(cpu->loadStoreQueue,cpu->dispatch);  //for LS ins
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

  if (cpu->issueQueue[0].free_bit == 1)
  {
    for (int i = 0; i < IQ_SIZE; i++)
    {
      if (cpu->issueQueue[i].free_bit == 1)
      {
        if (cpu->mulFU.has_insn == 0 && strcmp(cpu->issueQueue[i].opcode_str, "MUL") == 0)
        {
          cpu->mulFU.pc = cpu->issueQueue[i].pc;
          strcpy(cpu->mulFU.opcode_str, cpu->issueQueue[i].opcode_str);
          cpu->mulFU.opcode = cpu->issueQueue[i].opcode;
          cpu->mulFU.rs1 = cpu->issueQueue[i].rs1;
          cpu->mulFU.rs2 = cpu->issueQueue[i].rs2;
          cpu->mulFU.rs3 = cpu->issueQueue[i].rs3;
          cpu->mulFU.rd = cpu->issueQueue[i].rd;
          cpu->mulFU.imm = cpu->issueQueue[i].imm;
          cpu->mulFU.rs1_value = cpu->issueQueue[i].rs1_value;
          cpu->mulFU.rs2_value = cpu->issueQueue[i].rs2_value;
          cpu->mulFU.rs3_value = cpu->issueQueue[i].rs3_value;
          cpu->mulFU.result_buffer = cpu->issueQueue[i].result_buffer;
          cpu->mulFU.memory_address = cpu->issueQueue[i].memory_address;
          cpu->mulFU.has_insn = cpu->issueQueue[i].has_insn;
          cpu->mulFU.stalled = cpu->issueQueue[i].stalled;
          cpu->mulFU.free_bit = cpu->issueQueue[i].free_bit;
          cpu->mulFU.src1_validbit = cpu->issueQueue[i].src1_validbit;
          cpu->mulFU.src1_tag = cpu->issueQueue[i].src1_tag;
          cpu->mulFU.src1_value = cpu->issueQueue[i].src1_value;
          cpu->mulFU.src2_validbit = cpu->issueQueue[i].src2_validbit;
          cpu->mulFU.src2_tag = cpu->issueQueue[i].src2_tag;
          cpu->mulFU.src2_value = cpu->issueQueue[i].src2_value;
          cpu->mulFU.src3_validbit = cpu->issueQueue[i].src3_validbit;
          cpu->mulFU.src3_tag = cpu->issueQueue[i].src3_tag;
          cpu->mulFU.src3_value = cpu->issueQueue[i].src3_value;
          cpu->mulFU.dtype = cpu->issueQueue[i].dtype;
          cpu->mulFU.dest = cpu->issueQueue[i].dest;
          cpu->mulFU.has_insn = 1;
          print_issueQueue(cpu);
          for (int j = i; j < 8; j++)
          {
            cpu->issueQueue[i] = cpu->issueQueue[j - 1];
          }
        }
        else if (cpu->logicalopFU.has_insn == 0 && (strcmp(cpu->issueQueue[i].opcode_str, "OR") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "EX-OR") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "AND") == 0))
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
          print_issueQueue(cpu);
          for (int j = i; j < 8; j++)
          {
            cpu->issueQueue[i] = cpu->issueQueue[j - 1];
          }
        }
        else if (cpu->intFU.has_insn == 0 && (strcmp(cpu->issueQueue[i].opcode_str, "MOVC") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "ADD") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "ADDL") == 0 ||
                                              strcmp(cpu->issueQueue[i].opcode_str, "SUB") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "SUBL") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "DIV") == 0 ||
                                              strcmp(cpu->issueQueue[i].opcode_str, "JUMP") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "BZ") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "BNZ") == 0 ||
                                              strcmp(cpu->issueQueue[i].opcode_str, "CMP") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "NOP") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "HALT") == 0 ||
                                              strcmp(cpu->issueQueue[i].opcode_str, "LOAD") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "LDR") == 0 || strcmp(cpu->issueQueue[i].opcode_str, "STORE") == 0 ||
                                              strcmp(cpu->issueQueue[i].opcode_str, "STR") == 0))
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
          print_issueQueue(cpu);
          for (int j = i; j < 8; j++)
          {
            cpu->issueQueue[i] = cpu->issueQueue[j - 1];
          }
        }
      }
    }
  }
  cpu->reorderBuffer->array->free_bit = 1;
  cpu->reorderBuffer->array->pc = cpu->dispatch.pc;
}
void UpdateROB(APEX_CPU *cpu){
if(isROBFull(cpu->reorderBuffer)==0){
  for (int i = cpu->reorderBuffer->head; i <= cpu->reorderBuffer->tail; i++)
  {
    if (cpu->reorderBuffer->array[i].rob_valid == 0)
    {
      cpu->reorderBuffer->array[i].rob_valid = 1;         // making rob entry invalid
      cpu->reorderBuffer->array[i].free_bit = 1;          // rob allocated bit 1
      cpu->reorderBuffer->array[i].pc = cpu->dispatch.pc; // assigning pc value
      cpu->reorderBuffer->array[i].dest = cpu->dispatch.dest;
      cpu->reorderBuffer->array[i].rd = cpu->dispatch.rd;
      cpu->reorderBuffer->array[i].opcode = cpu->dispatch.opcode;
      cpu->reorderBuffer->array[i].rs1 = cpu->dispatch.rs1;
      cpu->reorderBuffer->array[i].rs2 = cpu->dispatch.rs2;
      cpu->reorderBuffer->array[i].rs3 = cpu->dispatch.rs3;
      cpu->reorderBuffer->array[i].imm = cpu->dispatch.imm;
      strcpy(cpu->reorderBuffer->array[i].opcode_str,cpu->dispatch.opcode_str);
      break;
      // lsq index is pending
    }
  }
}
   if (isROBEmpty(cpu->reorderBuffer) == 0){
    printf("ROB contents are:\n");
   for (int i = cpu->reorderBuffer->head; i <= cpu->reorderBuffer->tail; i++){
   if (cpu->reorderBuffer->array[i].rob_valid == 1){
     printf("%d.", i+1);

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
        printf("%s,R%d,R%d\n", cpu->reorderBuffer->array[i].opcode_str, cpu->reorderBuffer->array[i].rs1, cpu->reorderBuffer->array[i].rs2);
        break;
      }
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

  int array1[2];
  if (cpu->intFU.has_insn && cpu->intFU.stalled == 0)
  {
    /* intFU logic based on instruction type */
    switch (cpu->intFU.opcode)
    {
    case OPCODE_ADD:
    {
      printf("before its working and P2 value %d and P3 value %d\n", cpu->intFU.src1_value, cpu->intFU.src2_value);
      if (cpu->intFU.src1_validbit == 1)
      {
        cpu->intFU.src1_value = cpu->physicalregs[cpu->rename_table[cpu->intFU.rs1]].value;
        cpu->physicalregs[cpu->rename_table[cpu->intFU.rs1]].isValid = 0;
      }
      if (cpu->intFU.src2_validbit == 1)
      {
        cpu->intFU.src2_value = cpu->physicalregs[cpu->rename_table[cpu->intFU.rs2]].value;
        cpu->physicalregs[cpu->rename_table[cpu->intFU.rs2]].isValid = 0;
      }
      printf("its working and P2 value %d and P3 value %d\n", cpu->intFU.src1_value, cpu->intFU.src2_value);
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

    case OPCODE_BZ:
    {

      if (cpu->zero_flag == TRUE)
      {
        cpu->pc = cpu->intFU.pc + cpu->intFU.imm;

        cpu->fetch_from_next_cycle = TRUE;
        /* Decode and dispatch Flushed */
        cpu->dispatch.has_insn = FALSE;
        cpu->decode.has_insn = FALSE;
        /* Make sure fetch stage is enabled to start fetching from new PC */
        cpu->fetch.has_insn = TRUE;
        cpu->dispatch.stalled = 0;
        cpu->decode.stalled = 0;
        cpu->fetch.stalled = 0;
      }
      break;
    }

    case OPCODE_BNZ:
    {

      if (cpu->zero_flag == FALSE)
      {
        /* Calculate new PC, and send it to fetch unit */
        cpu->pc = cpu->intFU.pc + cpu->intFU.imm;

        cpu->fetch_from_next_cycle = TRUE;

        /* Decode and dispatch Flushed */
        cpu->dispatch.has_insn = FALSE;
        cpu->decode.has_insn = FALSE;

        /* Make sure fetch stage is enabled to start fetching from new PC */
        cpu->fetch.has_insn = TRUE;
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
    case OPCODE_JUMP:
    {
      cpu->pc = cpu->intFU.pc + (cpu->intFU.imm + cpu->intFU.src1_value);
      cpu->fetch_from_next_cycle = TRUE;
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
    case OPCODE_LDR:
    {
      cpu->intFU.memory_address = cpu->intFU.src1_value + cpu->intFU.src2_value;

      break;
    }
    case OPCODE_LOAD:
    {
      cpu->intFU.memory_address = cpu->intFU.src1_value + cpu->intFU.imm;

      break;
    }
    case OPCODE_STORE:
    {
      cpu->intFU.result_buffer = cpu->intFU.rs1_value;

      cpu->intFU.memory_address = cpu->intFU.rs2_value + cpu->intFU.imm;
      break;
    }

    case OPCODE_STR:
    {
      cpu->intFU.result_buffer = cpu->intFU.rs1_value;

      cpu->intFU.memory_address = cpu->intFU.rs2_value + cpu->intFU.rs3_value;

      break;
    }
    default:
    {
      break;
    }
    }

    array1[0] = cpu->intFU.dest;
    array1[1] = cpu->intFU.result_buffer;

    add_at_rear(&cpu->head, array1);
    cpu->intFU.has_insn = FALSE;

    if (ENABLE_DEBUG_MESSAGES)
    {
      print_fun_unit("intFU", &cpu->intFU);
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

static void APEX_mulFU(APEX_CPU *cpu)
{
  if (cpu->mulFU.has_insn && cpu->mulFU.stalled == 0)
  {
    /* mulFU logic based on instruction type */

    switch (cpu->mulFU.opcode)
    {
    case OPCODE_MUL:
    {
      cpu->mulFU.result_buffer = cpu->mulFU.rs1_value * cpu->mulFU.rs2_value;
      /* Set the zero flag based on the result buffer */
      if (cpu->mulFU.result_buffer == 0)
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
    /* Copy data from mulFU latch to memory latch*/
    // cpu->memory = cpu->mulFU;
    cpu->mulFU.has_insn = FALSE;

    if (ENABLE_DEBUG_MESSAGES)
    {
      print_stage_content("mulFU", &cpu->mulFU);
    }
  }
  else
  {
    if (ENABLE_DEBUG_MESSAGES)
    {
      print_stage_empty("mulFU");
    }
  }
}
static void APEX_logicalopFU(APEX_CPU *cpu)
{
  if (cpu->logicalopFU.has_insn && cpu->logicalopFU.stalled == 0)
  {
    /* logicalopFU logic based on instruction type */

    switch (cpu->logicalopFU.opcode)
    {
    case OPCODE_AND:
    {
      cpu->logicalopFU.result_buffer = cpu->logicalopFU.rs1_value & cpu->logicalopFU.rs2_value;

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
      cpu->logicalopFU.result_buffer = cpu->logicalopFU.rs1_value | cpu->logicalopFU.rs2_value;

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
      cpu->logicalopFU.result_buffer = cpu->logicalopFU.rs1_value ^ cpu->logicalopFU.rs2_value;
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
    /* Copy data from logicalopFU latch to memory latch*/
    // cpu->memory = cpu->logicalopFU;
    cpu->logicalopFU.has_insn = FALSE;

    if (ENABLE_DEBUG_MESSAGES)
    {
      print_stage_content("logicalopFU", &cpu->logicalopFU);
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

void list_to_bus(APEX_CPU *cpu)
{
  if (isEmpty(cpu->head) == 0)
  {
    if (cpu->bus0.busy == 0)
    {
      Forwarding_Bus_0_tagpart(cpu);
    }
  }
  if (isEmpty(cpu->head) == 0)
  {
    if (cpu->bus1.busy == 0)
    {
      Forwarding_Bus_1_tagpart(cpu);
    }
  }
}
// bus0
void Forwarding_Bus_0_tagpart(APEX_CPU *cpu)
{
  cpu->bus0.busy = 1;
  cpu->bus0.tag_part = cpu->head->data_values[0];
  // enable datapart foor next cycle and use next_dat_bus0
  cpu->bus0.bus_was_busy = 1;
  cpu->bus0.next_data_bus = cpu->head->data_values[1];
  // remove from the list
  int returnarray2[2];
  int *returned_data2;
  remove_at_front(&cpu->head, returnarray2);
  cpu->bus0.busy = 0;
}
void Forwarding_Bus_0_datapart(APEX_CPU *cpu)
{
  if (cpu->bus0.bus_was_busy == 1)
  {
    cpu->bus0.data_part = cpu->bus0.next_data_bus;
    print_bus0(cpu);
    cpu->physicalregs[cpu->bus0.tag_part].value = cpu->bus0.data_part;
    cpu->bus0.bus_was_busy = 0;
  }
}
// bus1
void Forwarding_Bus_1_tagpart(APEX_CPU *cpu)
{
  cpu->bus1.busy = 1;
  cpu->bus1.tag_part = cpu->head->data_values[0];
  // enable datapart foor next cycle and use next_dat_bus0
  cpu->bus1.bus_was_busy = 1;
  cpu->bus1.next_data_bus = cpu->head->data_values[1];
  print_bus1(cpu);
  // remove from the list
  int returnarray[2];
  int *returned_data;
  returned_data = remove_at_front(&cpu->head, returnarray);
  cpu->bus1.busy = 0;
}
void Forwarding_Bus_1_datapart(APEX_CPU *cpu)
{
  if (cpu->bus1.bus_was_busy == 1)
  {
    cpu->bus1.data_part = cpu->bus1.next_data_bus;
    print_bus1(cpu);
    cpu->bus1.bus_was_busy = 0;
  }
}

/*
 * Memory Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation

static void APEX_memory(APEX_CPU *cpu){
  if (cpu->memory.has_insn)
  {
    switch (cpu->memory.opcode)
    {
    case OPCODE_LOAD:
    case OPCODE_LDR:
    {

      cpu->memory.result_buffer = cpu->data_memory[cpu->memory.memory_address];
      cpu->decode.stalled = 0;
      break;
    }

    case OPCODE_STORE:
    case OPCODE_STR:
    {

      cpu->data_memory[cpu->memory.memory_address] = cpu->memory.result_buffer;
      break;
    }
    case OPCODE_HALT:
    {

      cpu->intFU.has_insn = 0;
      break;
    }

    default:
    {
      break;
    }
    }

    cpu->writeback = cpu->memory;
    cpu->memory.has_insn = FALSE;

    if (ENABLE_DEBUG_MESSAGES)
    {
      print_stage_content("Memory", &cpu->memory);
    }
  }
  else
  {
    if (ENABLE_DEBUG_MESSAGES)
    {
      print_stage_empty("Memory");
    }
  }
}
*/
int commit(APEX_CPU* cpu) {
   int i=0;
   if(cpu->reorderBuffer->array[i].rob_valid==0 && cpu->physicalregs[cpu->reorderBuffer->array[i].dest].isValid==0){
   switch (cpu->reorderBuffer->array[i].opcode){
    case OPCODE_ADD:
    case OPCODE_LOAD:
    case OPCODE_MOVC:
    case OPCODE_ADDL:
    case OPCODE_SUB:
    case OPCODE_SUBL:
    case OPCODE_DIV:
    case OPCODE_LDR:
    {
      cpu->regs[cpu->reorderBuffer->array[i].rd] = cpu->reorderBuffer->array[i].result_buffer;
      break;
    }
    case OPCODE_MUL:
    {
      cpu->regs[cpu->reorderBuffer->array[i].rd] = cpu->reorderBuffer->array[i].result_buffer;
      break;
    }
    case OPCODE_AND:
    case OPCODE_OR:
    case OPCODE_XOR:
    {
      cpu->regs[cpu->reorderBuffer->array[i].rd] = cpu->reorderBuffer->array[i].result_buffer;
      break;
    }
    case OPCODE_STORE:
    case OPCODE_STR:
    case OPCODE_BZ:
    case OPCODE_JUMP:
    case OPCODE_BNZ:
    case OPCODE_NOP:
    case OPCODE_CMP:
    {


      break;
    }
   case OPCODE_HALT:{ 
     return 1;
    break;
    }
    default:
    {
      break;
    }
    }
    //delete at the head of ROB
    cpu->reorderBuffer->head = (cpu->reorderBuffer->head + 1)%cpu->reorderBuffer->capacity;
    cpu->reorderBuffer->size = cpu->reorderBuffer->size - 1;
    cpu->insn_completed++;
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
  cpu->mulFU.stalled = 0;
  cpu->logicalopFU.stalled = 0;
  // cpu->memory.stalled = 0;
  // cpu->writeback.stalled = 0;
  /* Parse input file and create code memory */
  cpu->code_memory = create_code_memory(filename, &cpu->code_memory_size);
  if (!cpu->code_memory)
  {
    free(cpu);
    return NULL;
  }

  // new code

  // int physicalreg[PREG_FILE_SIZE][3]; // physical registers- valid bit-0 data value -1 cc flag value -2
  // int renametable[REG_FILE_SIZE+1][1000];
  cpu->freePhysicalRegister = createQueue(15);
  cpu->reorderBuffer = createROBQueue(12);
  cpu->loadStoreQueue=createLSQQueue(8);
  for (int i = 0; i < 15; i++)
  {
    insertRegister(cpu->freePhysicalRegister, i);
  }
  memset(cpu->issueQueue, 0, sizeof(APEX_Instruction) * 8);
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
void APEX_cpu_run(APEX_CPU *cpu)
{
  char user_prompt_val;

  while (TRUE)
  {
    if (ENABLE_DEBUG_MESSAGES)
    {
      printf("--------------------------------------------\n");
      printf("Clock Cycle #: %d\n", cpu->clock + 1);
      printf("--------------------------------------------\n");
    }
    if(commit(cpu)){
      printf("APEX_CPU: Simulation Stopped, cycles = %d instructions = %d\n", cpu->clock + 1, cpu->insn_completed);
      break;
    }
    if (cpu->bus1.bus_was_busy == 1)
    {
      Forwarding_Bus_1_datapart(cpu);
    }
    if (cpu->bus0.bus_was_busy == 1)
    {
      Forwarding_Bus_0_datapart(cpu);
    }
    list_to_bus(cpu);

    printList(cpu->head);
    APEX_mulFU(cpu);
    APEX_logicalopFU(cpu);
    APEX_intFU(cpu);
    APEX_issueQueueUpdate(cpu);
    APEX_dispatch(cpu);
    APEX_decode(cpu);
    APEX_fetch(cpu);

    printf("\nFlag contents: Z = %d\n\n", cpu->zero_flag);
    printContentsOfFreeRegistersList(cpu);
    // print_physical_register_state(cpu);
    print_rename_table(cpu);
    // print_data_memory(cpu);

    if (cpu->single_step)
    {
      printf("Press any key to next CPU clock or <q> to quit:\n");
      scanf("%c", &user_prompt_val);

      if ((user_prompt_val == 'Q') || (user_prompt_val == 'q'))
      {
        printf("APEX_CPU: Simulation Stopped, cycles = %d instructions = %d\n", cpu->clock + 1, cpu->insn_completed);
        break;
      }
    }

    cpu->clock++;
  }
  printf("APEX_CPU: Simulation Complete, cycles = %d instructions = %d\n", cpu->clock + 1, cpu->insn_completed);
  print_physical_register_state(cpu);
  print_reg_file(cpu);
  print_register_state(cpu);
  print_data_memory(cpu);
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

/*
 * Register Queue
 * */

int isRegisterQueueFull(struct PhysicalRegistersQueue *queue)
{
  return (queue->size == queue->capacity);
}

int isRegisterQueueEmpty(struct PhysicalRegistersQueue *queue)
{
  return (queue->size == 0);
}

void insertRegister(struct PhysicalRegistersQueue *queue, int item)
{
  if (isRegisterQueueFull(queue))
  {
    return;
  }
  queue->rear = (queue->rear + 1) % queue->capacity;
  queue->array[queue->rear] = item;
  queue->size = queue->size + 1;
}

int getRegister(struct PhysicalRegistersQueue *queue)
{
  if (isRegisterQueueEmpty(queue))
    return -1;
  int item = queue->array[queue->front];
  queue->front = (queue->front + 1) % queue->capacity;
  queue->size = queue->size - 1;
  // doubtful code
  // if(cpu->decode.stalled==1){
  // cpu->decode.stalled=0;}
  //
  return item;
}

int registerQueueFront(struct PhysicalRegistersQueue *queue)
{
  if (isRegisterQueueEmpty(queue))
    return -1;
  return queue->array[queue->front];
}

int registerQueueRear(struct PhysicalRegistersQueue *queue)
{
  if (isRegisterQueueEmpty(queue))
    return -1;
  return queue->array[queue->rear];
}

/*
 * Issue Queue
 * */

int isIssueQueueFull(APEX_CPU *cpu)
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
// ROB
int isROBFull(struct ReorderBuffer *queue)
{
  if (queue->size == queue->capacity)
  {
    return 1;
  }
  return 0;
}
int isROBEmpty(struct ReorderBuffer *queue)
{
  if (queue->size != 0)
  {
    return 1;
  }
  return 0;
}
//LSQ
int isLSQFull(struct LSQ* queue){
    if (queue->size == queue->capacity)
  {
    return 1;
  }
  return 0;
}
int isLSQEmpty(struct LSQ* queue){
    if (queue->size != 0)
  {
    return 1;
  }
  return 0;
}