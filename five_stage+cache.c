
/**************************************************************/
/* CS/COE 1541
   Project 2 5-Stage Pipeline Simulator with Cache simulation
   Collaborators: Zachary Whitney, Albert Yang, Jeremy Kato
   IDS: zdw9, aly31, jdk81
   compile with gcc -o pipeline five_stage.c
   and execute using
   ./pipeline  /afs/cs.pitt.edu/courses/1541/short_traces/sample.tr 0
***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <string.h>
#include "CPU.h"
#include "cache.h"
#include "queue.h"

unsigned char get_prediction(struct instruction *instr);
void update_prediction(struct instruction *instr, unsigned char new_prediction);
unsigned int check_data_hazard(struct instruction *first_instr, struct instruction *x);
unsigned int check_control_hazard(struct instruction *first_instr, struct instruction *x);
void insert_noop(struct instruction *loc);
void insert_squashed(struct instruction *loc);
unsigned int is_branch_taken(struct instruction *branch_instr, struct instruction *next_instr);
unsigned char is_right_target(struct instruction *instr);

struct prediction ht[HASH_TABLE_SIZE];
struct instruction PREFETCH[2];
struct zqueue write_buffer;             //the actual write buffer

// cache statistics
unsigned int I_accesses = 0;    //instruction cache hits + misses
unsigned int I_misses = 0;
unsigned int D_accesses = 0;    //data cache hits + misses
unsigned int D_misses = 0;
unsigned int WB_accesses = 0;      
unsigned int d_writebacks = 0;  //# of times we write back from L1 to L2
unsigned int L2_accesses = 0;   
unsigned int WB_N1 = 0;         //# of times we find an L1 miss in the write buffer before checking L2
unsigned int WB_N2 = 0;         //# of times we evict a dirty block but the write buffer is full

int main(int argc, char **argv)
{
  FILE * config;
  config = fopen("cache_config.txt", "r");
  if(config == NULL) 
  {
      perror("Error opening file");
      return(-1);
  }

  unsigned int I_size;          //size of the instruction cache, in KB
  unsigned int I_assoc;         //associativity of the instruction cache
  unsigned int D_size;          //size of the data cache, in KB
  unsigned int D_assoc;         //associativity of the data cache
  unsigned int B_size;          //block size for both L1 caches and the L2 cache
  unsigned int WB_size;         //size of the write buffer in entries (0 means no buffer)
  unsigned int miss_penalty;    //the number of cycles it takes to access the L2 cache

  if(fscanf(config, "%d %d %d %d %d %d %d", &I_size, &I_assoc, &D_size, &D_assoc, &B_size, &WB_size,&miss_penalty) <= 0)
  {
      printf("Could not parse file properly\n");
      exit(-1);
  }
  fclose(config);

  struct instruction *tr_entry;
  struct instruction IF, ID, EX, MEM, WB;
  size_t size;
  char *trace_file_name;
  int trace_view_on = 0;
  int prediction_method = 0;
  int squash_counter = 0;
  int flush_counter = 6; //5 stage pipeline and 2-part buffer, so we have to move 6 instructions once trace is done
  unsigned int l2_used = 0;
  int cycle_number = -2;  //start at -2 to ignore filling the PREFETCH QUEUE
  int buffer_stalled = 0;
  int access_result;

  memset(ht, 0, HASH_TABLE_SIZE * sizeof(struct prediction));

  if (argc == 1) {
    fprintf(stdout, "\nUSAGE: tv <trace_file> <prediction method> <switch - any character> \n");
    fprintf(stdout, "\n(prediction method) 0 to predict NOT TAKEN, 1 to predict TAKEN.\n\n");
    fprintf(stdout, "\n(switch) to turn on or off individual item view.\n\n");
    exit(0);
  }

  trace_file_name = argv[1];
  if (argc > 2) prediction_method = atoi(argv[2]);
  if (argc > 3) trace_view_on = atoi(argv[3]);

  fprintf(stdout, "\n ** opening file %s\n", trace_file_name);

  trace_fd = fopen(trace_file_name, "rb");

  if (!trace_fd) {
    fprintf(stdout, "\ntrace file %s not opened.\n\n", trace_file_name);
    exit(0);
  }

  trace_init();

  //construct the data and instruction caches
  struct cache_t *I_cache;
  I_cache = cache_create(I_size, B_size, I_assoc);
  struct cache_t *D_cache;
  D_cache = cache_create(D_size, B_size, D_assoc);

  if(WB_size)
    initialize_queue(&write_buffer, WB_size);

  unsigned int evicted_block; //holds the address of an evicted block, if any

  while(1) {
    
    int has_data_hazard = check_data_hazard(&PREFETCH[0], &PREFETCH[1]);

    if(prediction_method == 0 && check_control_hazard(&PREFETCH[0], &PREFETCH[1]))
      squash_counter++;

    if(prediction_method == 1 && PREFETCH[0].type == ti_BRANCH)
    {
      int branch_result = is_branch_taken(&PREFETCH[0], &PREFETCH[1]);
      if (get_prediction(&PREFETCH[0]) != branch_result || (branch_result == TAKEN && !is_right_target(&PREFETCH[0])))
        squash_counter++;
      update_prediction(&PREFETCH[0], branch_result);
    }
    else if (prediction_method == 1 && (PREFETCH[0].type == ti_JRTYPE || PREFETCH[0].type == ti_JTYPE))
    {
      if(!is_right_target(&PREFETCH[0]))
        squash_counter++;
      update_prediction(&PREFETCH[0], TAKEN);
    }

    if(!has_data_hazard && !squash_counter)
      size = trace_get_item(&tr_entry); /* put the instruction into a buffer */

    if (!size && flush_counter==0) 
    {       /* no more instructions (instructions) to simulate */

      //consume any remaining write buffer entries

      cycle_number += l2_used;
      while(write_buffer.size > 0)
      {
        dequeue(&write_buffer);
        cycle_number += miss_penalty;
        L2_accesses++;
      }

      double i_missrate = 100 *((double)I_misses/I_accesses);
      double d_missrate = 0;
      if(D_accesses > 0)
        d_missrate = 100 *((double)D_misses/D_accesses);
      printf("+ Simulation terminates at cycle : %u\n", cycle_number);
      printf("L1 Data cache:\t\t%u accesses, %u hits, %u misses, %0.4f%% miss rate, %u write backs\n", D_accesses, (D_accesses - D_misses), D_misses, d_missrate, d_writebacks);
      printf("L1 Instruction cache:\t%u accesses, %u hits, %u misses, %0.4f%% miss rate\n",I_accesses, (I_accesses - I_misses), I_misses, i_missrate);
      printf("L2 cache:\t\t%u accesses\n", L2_accesses);
      printf("Write Buffer:\t\t%u N1, %u N2\n", WB_N1, WB_N2);
      break;
    }
    else{              /* move the pipeline forward */

      if(l2_used == 1)
        dequeue(&write_buffer);
      if(l2_used)  //memory makes progress if in use
        l2_used--;
      
      cycle_number++;

      /* move instructions one stage ahead */
      WB = MEM;
      MEM = EX;
      EX = ID;
      ID = IF;
      IF = PREFETCH[0];

      if(squash_counter)
        insert_squashed(&PREFETCH[0]);
      else if(has_data_hazard)
        insert_noop(&PREFETCH[0]);
      else
        PREFETCH[0] = PREFETCH[1];

      if(!size){    /* if no more instructions in trace, feed NOOPS and reduce flush_counter */
        insert_noop(&PREFETCH[1]);
        flush_counter--;
      }
      else
      {
        if(!has_data_hazard && !squash_counter)
        {
            memcpy(&PREFETCH[1], tr_entry , sizeof(IF)); //get next instruction
            if(IF.type != ti_NOP && IF.type != ti_SQUASHED)
            {
              access_result = cache_access(I_cache, IF.PC, 0, &evicted_block); //check for a hit
              I_accesses++;
            }
            else
              access_result = 0;
            
            if (access_result > 0)    /* stall the pipe if instruction fetch returns a miss */
            {
                if(l2_used)
                    dequeue(&write_buffer);
                cycle_number += miss_penalty + l2_used;
                l2_used = 0;
                buffer_stalled = 1;
                I_misses++;
                L2_accesses++;
            }
        }
      }

      if(squash_counter)
        squash_counter--;

          if(MEM.type == ti_LOAD)
          {
            D_accesses++;
            access_result = cache_access(D_cache, MEM.Addr, 0, &evicted_block);
            if(access_result > 0)
            {
                D_misses++;
                if(WB_size)
                {
                    cycle_number++; //check write buffer penalty
                    if(contains( &write_buffer, (MEM.Addr / B_size) )) //miss found in WB
                    {
                        WB_N1++;
                        access_result = 0; //now it's an effective hit
                    }
                }
            }
            if (access_result == 1)
            {
                if(l2_used)
                    dequeue(&write_buffer);
                cycle_number += miss_penalty + l2_used;
                l2_used = 0;
                buffer_stalled = 1;
                L2_accesses++;
            }
            else if (access_result == 2)
            {
                d_writebacks++;
                if(WB_size)
                {
                    if(write_buffer.size < write_buffer.capacity)
                      enqueue(&write_buffer, evicted_block);
                    else
                    {
                      dequeue(&write_buffer);
                      if(!l2_used)
                         l2_used += miss_penalty;
                      cycle_number += miss_penalty + l2_used;
                      l2_used = 0;
                      buffer_stalled = 1;
                      enqueue(&write_buffer, evicted_block);
                      WB_N2++;
                      L2_accesses++;
                    }
                }
                else
                { //need to writeback now if no write buffer
                    cycle_number += miss_penalty;
                    L2_accesses++;
                }
                if(l2_used)
                    dequeue(&write_buffer);
                cycle_number += miss_penalty + l2_used;
                l2_used = 0;
                buffer_stalled = 1;
                L2_accesses++;
            }
          }
          else if(MEM.type == ti_STORE)
          {
            D_accesses++;
            access_result = cache_access(D_cache, MEM.Addr, 1, &evicted_block);
            if(access_result > 0)
            {
                D_misses++;
                if(WB_size)
                {
                    cycle_number++; //check write buffer penalty
                    if(contains( &write_buffer, (MEM.Addr / B_size) )) //miss found in WB
                    {
                        WB_N1++;
                        access_result = 0; //now it's an effective hit
                    }
                }
            }
            if (access_result == 1)
            {
                if(l2_used)
                    dequeue(&write_buffer);
                cycle_number += miss_penalty + l2_used;
                l2_used = 0;
                buffer_stalled = 1;
                L2_accesses++;
            }
            else if (access_result == 2)
            {
                d_writebacks++;
                if(WB_size)
                {
                    if(write_buffer.size < WB_size)
                        enqueue(&write_buffer, evicted_block);
                    else
                    {
                        dequeue(&write_buffer);
                        if(!l2_used)
                          l2_used += miss_penalty;
                        cycle_number += miss_penalty + l2_used;
                        l2_used = 0;
                        buffer_stalled = 1;
                        enqueue(&write_buffer, evicted_block);
                        WB_N2++;
                        L2_accesses++;
                    }
                }
                else
                { //need to writeback now if no write buffer
                    cycle_number += miss_penalty;
                    L2_accesses++;
                }
                if(l2_used)
                    dequeue(&write_buffer);
                cycle_number += miss_penalty + l2_used;
                l2_used = 0;
                buffer_stalled = 1;
                L2_accesses++;
            }
          }

            if(WB_size && !l2_used && !buffer_stalled && write_buffer.size > 0)
            {
                l2_used += miss_penalty;
                L2_accesses++;
            }

            buffer_stalled = 0;

      //printf("==============================================================================\n");

  }
    if (trace_view_on && cycle_number>=5) {/* print the executed instruction if trace_view_on=1 */
      switch(WB.type) {
        case ti_NOP:
          //DON'T PRINT NO OPS IN THIS VERSION OF THE SIMULATION
          //printf("[cycle %d] NOP:\n",cycle_number) ;
          break;
        case ti_RTYPE: /* registers are translated for printing by subtracting offset  */
          printf("[cycle %d] RTYPE:",cycle_number) ;
          printf(" (PC: %d)(sReg_a: %d)(sReg_b: %d)(dReg: %d) \n", WB.PC, WB.sReg_a, WB.sReg_b, WB.dReg);
          break;
        case ti_ITYPE:
          printf("[cycle %d] ITYPE:",cycle_number) ;
          printf(" (PC: %d)(sReg_a: %d)(dReg: %d)(addr: %d)\n", WB.PC, WB.sReg_a, WB.dReg, WB.Addr);
          break;
        case ti_LOAD:
          printf("[cycle %d] LOAD:",cycle_number) ;
          printf(" (PC: %d)(sReg_a: %d)(dReg: %d)(addr: %d)\n", WB.PC, WB.sReg_a, WB.dReg, WB.Addr);
          break;
        case ti_STORE:
          printf("[cycle %d] STORE:",cycle_number) ;
          printf(" (PC: %d)(sReg_a: %d)(sReg_b: %d)(addr: %d)\n", WB.PC, WB.sReg_a, WB.sReg_b, WB.Addr);
          break;
        case ti_BRANCH:
          printf("[cycle %d] BRANCH:",cycle_number) ;
          printf(" (PC: %d)(sReg_a: %d)(sReg_b: %d)(addr: %d)\n", WB.PC, WB.sReg_a, WB.sReg_b, WB.Addr);
          break;
        case ti_JTYPE:
          printf("[cycle %d] JTYPE:",cycle_number) ;
          printf(" (PC: %d)(addr: %d)\n", WB.PC,WB.Addr);
          break;
        case ti_SPECIAL:
          printf("[cycle %d] SPECIAL:\n",cycle_number) ;
          break;
        case ti_JRTYPE:
          printf("[cycle %d] JRTYPE:",cycle_number) ;
          printf(" (PC: %d) (sReg_a: %d)(addr: %d)\n", WB.PC, WB.dReg, WB.Addr);
          break;
        case ti_SQUASHED:
          //DON'T PRINT NO OPS IN THIS VERSION OF THE SIMULATION
          //printf("[cycle %d] SQUASHED\n",cycle_number) ;
          break;
      }
    }
  }

  trace_uninit();

  exit(0);
}

//predicts last choice for a given branch if stored in predictor, else predicts NOT TAKEN
unsigned char get_prediction(struct instruction *instr)
{
  struct prediction p = ht[HASH(instr->PC)];
  return p.taken;
}

//checks a jump or branch target address versus the target in the hash table
//returns 1 if they match, otherwise returns 0
unsigned char is_right_target(struct instruction *instr)
{
  struct prediction p = ht[HASH(instr->PC)];
  return instr->Addr == p.target;
}

//updates the BTB for given instruction
void update_prediction(struct instruction *instr, unsigned char new_prediction)
{
  int index = HASH(instr->PC);
  struct prediction p;
  p.taken = new_prediction;
  p.target =  instr->Addr;
  ht[index] = p;
}

//returns 1 if a data hazard is found, else returns 0
//x is assumed to be the possibly dependent instruction following the LOAD
unsigned int check_data_hazard(struct instruction *first_instr, struct instruction *x)
{
  if(first_instr->type != ti_LOAD)
    return 0;
  int hazard_register = first_instr->dReg;
  if(x->sReg_a == hazard_register && x->type != ti_JTYPE && x->type != ti_SPECIAL && x->type != ti_SQUASHED)
    return 1;
  if(x->sReg_b == hazard_register && (x->type == ti_RTYPE || x->type == ti_STORE || x->type == ti_BRANCH))
    return 1;

  return 0;
}

//returns 1 if a control hazard is found, else returns 0
//x is the instruction following the possible BRANCH
unsigned int check_control_hazard(struct instruction *first_instr, struct instruction *next_instr)
{
  if(first_instr->type == ti_JTYPE || first_instr->type == ti_JRTYPE)
    return 1;
  if(first_instr->type == ti_BRANCH)
    return is_branch_taken(first_instr, next_instr);
  return 0;
}

//puts a NO_OP in the desired location
void insert_noop(struct instruction *loc)
{
  struct instruction NO_OP;
  memset(&NO_OP, 0, sizeof(struct instruction));
  *loc = NO_OP;
}

//puts a NO_OP in the desired location
void insert_squashed(struct instruction *loc)
{
  struct instruction SQUASHED_OP;
  uint old_PC = loc->PC;
  memset(&SQUASHED_OP, 0, sizeof(struct instruction));
  SQUASHED_OP.type = ti_SQUASHED;
  SQUASHED_OP.PC = old_PC;
  *loc = SQUASHED_OP;
}

//returns 0 if not taken and 1 if taken
unsigned int is_branch_taken(struct instruction *branch_instr, struct instruction *next_instr)
{
  if (branch_instr->PC != next_instr->PC - 4)
    return 1;

  return 0;
}
