
char * desciption = "Welcome to SBR! a super simple RISC processor emulator for branch prediction\n\
\n\
It supports a few MIPS instructions:\n\
\n\
ADDU, SYS, ADDIU, ADDIU, ANDI, BLTZ, BNE, BEQ, and J.\n\
\n\
SYS is a little special, in this ISA it is just a way to exit and print the\n\
registers.\n\
You can print the register file by calling \"SYS $30, $0, 0\"\n\
You can exit by calling \"SYS $31, $0, 0\"\n\
\n\
This emulator is meant to showcase two branch prediction schemes, one is just a\n\
2-bit saturating counter. The second is a more complex two level branch\n\
predictor that can exploit some temporal locality.\n";


#define MEMSIZE (1<<16)
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include<string.h>
#define var(x) (#x)
#define N 8
#define M 8
#define debug 0

enum opcodes{
  ADDU,
  SYS,
  ADDIU,
  ANDI,
  BLTZ,
  BNE,
  BEQ,
  J,
};

enum type {
  JUMP,
  BRANCH,
  NEITHER
};

enum FSM_STATES {
NTS,NTW, TW, TS
};

struct inst_info {
  int inst_type;
  int taken;
};

int lookup(char inst[6]) {

  if (!strcmp(inst, "ADDU")) {
    return ADDU;
  }
  if (!strcmp(inst, "ADDIU")) {
    return ADDIU;
  }
  if (!strcmp(inst, "ANDI")) {
    return ANDI;
  }
  if (!strcmp(inst, "SYS")) {
    return SYS;
  }
  if (!strcmp(inst, "BLTZ")) {
    return BLTZ;
  }
  if (!strcmp(inst, "BNE")) {
    return BNE;
  }
  if (!strcmp(inst, "BEQ")) {
    return BEQ;
  }
  if (!strcmp(inst, "J")) {
    return J;
  }
  printf("bad opcode! %s, aborting\n",inst);
  abort();

}

const char * rlookup(int opcode) {

  switch(opcode) {
  case ADDU:
    return "ADDU";
  case ADDIU:
    return "ADDIU";
  case ANDI:
    return "ANDI";
  case SYS:
    return "SYS";
  case BEQ:
    return "BEQ";
  case BNE:
    return "BNE";
  case BLTZ:
    return "BLTZ";
  case J:
    return "J";
  }
  printf("bad opcode! %d, aborting\n",opcode);
  abort();

}



struct r_type {
  uint32_t opcode : 6;
  uint32_t rs: 5;
  uint32_t rt: 5;
  uint32_t rd: 5;
  uint32_t shamt: 5;
  uint32_t funct:6;
} __attribute__((packed)) __attribute__((__aligned__(4)));

struct i_type {
  uint32_t opcode : 6;
  uint32_t rs : 5;
  uint32_t rt : 5;
  uint32_t immediate:16;
} __attribute__((packed)) __attribute__((__aligned__(4)));

struct j_type {
  uint32_t opcode : 6;
  uint32_t address:26;
} __attribute__((packed)) __attribute__((__aligned__(4)));

struct instruction {
  uint32_t opcode : 6;
} __attribute__((__aligned__(4)));

struct machine {
  uint32_t pc;
  uint32_t registers[32];
  uint8_t memory[MEMSIZE];
  uint8_t bht[1<<N];
  uint8_t pht[1<<N];
  uint8_t bhsrt[1<<M];
};
int32_t sext(uint16_t imm) {
  return ((int32_t)((int16_t)imm));
}
int step_machine(struct machine * m, struct inst_info * info) {

  if (m->pc %4 != 0) {
    printf("ERROR: instruction %08X not 4-byte aligned, aborting\n", m->pc);
    abort();
  }
  int br_taken = 0;
  uint32_t next_pc = m->pc+4;
  struct instruction * inst = ((struct instruction*)(m->memory+m->pc));

  struct i_type * i = (struct i_type*)inst;
  int32_t imm_lui = i->immediate << 16;
  uint16_t imm = i->immediate;
  int32_t imm_br = sext(i->immediate);
  struct r_type * r = (struct r_type*)inst;
  struct j_type * j = (struct j_type*)inst;
  uint32_t addr = j->address << 2;
  info->inst_type = NEITHER;
  info->taken = 0;
  switch(inst->opcode) {
    case SYS:
      if (i->rs == 31) {
        return 1;
      }
      if (i->rs == 30) {
        printf("registers:\n");
        for (int index = 0; index < 32; index++) {
          printf("M[%d] = %d\n", index, m->registers[index]);
        }
        break;
      }
      m->registers[i->rt] = imm_lui;
      break;
    case ADDIU:
      m->registers[i->rs] = m->registers[i->rt] + sext(imm);
      break;
    case ANDI:
      m->registers[i->rs] = m->registers[i->rt] & sext(imm);
      break;
    case ADDU:
      m->registers[r->rd] = m->registers[r->rs] + m->registers[r->rt];
      break;
    case BLTZ:
      info->inst_type = BRANCH;
      if (((int32_t)m->registers[i->rs]) < 0) {
        next_pc = m->pc + imm_br;
        info->taken = 1;
      }
      break;
    case BEQ:
      info->inst_type = BRANCH;
      if (m->registers[i->rs] == m->registers[i->rt]) {
        next_pc  = m->pc +  imm_br;
        info->taken = 1;
      }
      break;
    case BNE:
      info->inst_type = BRANCH;
      if (m->registers[i->rs] != m->registers[i->rt]) {
        next_pc  = m->pc +  imm_br;
        info->taken = 1;
      }
      break;
    case J:
      info->inst_type = JUMP;
      next_pc = (next_pc & 0xF0000000) | addr;
      info->taken = 1;
      break;
    default:
      printf("unknown instuction at PC %X, aborting\n",m->pc);
      abort();
  }
  m->pc = next_pc;
  return 0;
}

void load_program(struct machine * m, struct instruction * insts, int size) {
  memcpy(m->memory, insts, size * sizeof(struct instruction));
  m->pc = 0;
}


void print_instruction(struct instruction * inst) {
  if (inst->opcode == ADDU) {
    struct r_type * r = (struct r_type*)inst;
    printf("ADDU $%d, $%d, $%d\n", r->rd, r->rs, r->rt);
  } else if (inst->opcode == ADDIU ||
            inst->opcode == SYS ||
            inst->opcode == ANDI ||
            inst->opcode == BLTZ ||
            inst->opcode == BEQ ||
            inst->opcode == BNE) {
    struct i_type * i = (struct i_type*)inst;
    printf("%s $%d, $%d, %d\n", rlookup(inst->opcode), i->rs, i->rt, (int16_t)i->immediate);
  } else if (inst->opcode == J) {
    struct j_type * j = (struct j_type*)inst;
    printf("J 0x%08X\n", j->address);
  }

}

void predict_branch(struct machine * m, struct inst_info * bht_info, struct inst_info * bhsrt_info) {
  int32_t ones = -1;
  uint32_t onesu = (uint32_t)ones;
  uint32_t mask = onesu >> (32-N);
  int bht_index = (m->pc >> 2) & mask;
  uint8_t fsm = m->bht[bht_index];
  bht_info->taken = !!(fsm & 2);

  ones = -1;
  onesu = (uint32_t)ones;
  mask = onesu >> (32-M);
  int bhsrt_index = (m->pc >> 2) & mask;
  int pattern = m->bhsrt[bhsrt_index];
  fsm = m->pht[pattern];
  bhsrt_info->taken = !!(fsm & 2);
}

void update_predictor(struct machine * m, struct inst_info * info, uint32_t last_pc) {
  // simple one level saturating counter predictor
  int32_t ones = -1;
  uint32_t onesu = (uint32_t)ones;
  uint32_t mask = onesu >> (32-N);
  int bht_index = (last_pc >> 2) & mask;
  uint8_t fsm = m->bht[bht_index];
  int taken = info->taken;
  if (fsm == NTS && taken) {
    fsm = NTW;
  } else if (fsm == NTW && taken) {
    fsm = TW;
  } else if (fsm == NTW && !taken) {
    fsm = NTS;
  } else if (fsm == TW && taken) {
    fsm = TS;
  } else if (fsm == TW && !taken) {
    fsm = NTW;
  } else if (fsm == TS && !taken) {
    fsm = TW;
  }
  m->bht[bht_index] = fsm;



  // two level BHT to to exploit temporal locality
  ones = -1;
  onesu = (uint32_t)ones;
  mask = onesu >> (32-M);
  int bhsrt_index = (last_pc >> 2) & mask;
  int pattern = m->bhsrt[bhsrt_index];
  fsm = m->pht[pattern];
  if (fsm == NTS && taken) {
    fsm = NTW;
  } else if (fsm == NTW && taken) {
    fsm = TW;
  } else if (fsm == NTW && !taken) {
    fsm = NTS;
  } else if (fsm == TW && taken) {
    fsm = TS;
  } else if (fsm == TW && !taken) {
    fsm = NTW;
  } else if (fsm == TS && !taken) {
    fsm = TW;
  }
  m->pht[pattern] = fsm;
  m->bhsrt[bhsrt_index] = (pattern >> 1) | (taken << (M-1));
}
int main(int argc, char ** argv) {

  //open and get the file handle
  FILE* fh;
  fh = fopen(argv[1], "r");

  //check if file exists
  if (fh == NULL){
      printf("file does not exists %s", argv[1]);
      return 0;
  }

  struct instruction instrucions[1<<14];
  int current_instruction = 0;
  //read line by line
  char line[300];
  int line_size = 300;
  while (fgets(line, line_size, fh) != NULL)  {
      if (!strncmp(line, var(ADDU), strlen(var(ADDU))) ||
          !strncmp(line, var(JR), strlen(var(JR)))) {
        if (debug) printf("R_TYPE : %s",line);
        char inst [6];
        int rs, rt, rd;
        sscanf(line, "%s $%d, $%d, %d",inst,&rd,&rs,&rt);
        if (debug) printf("PARSED R_TYPE : %s $%d, $%d, $%d\n", inst, rd, rs,rt);
        struct r_type r = {lookup(inst), rd,rs,rt};
        memcpy(instrucions+current_instruction, &r, sizeof(r));
        current_instruction ++;
      }
      if (!strncmp(line, var(BNE), strlen(var(BNE))) ||
          !strncmp(line, var(BEQ), strlen(var(BEQ))) ||
          !strncmp(line, var(SYS), strlen(var(SYS))) ||
          !strncmp(line, var(BEQZ), strlen(var(BEQZ))) ||
          !strncmp(line, var(ANDI), strlen(var(ANDI))) ||
          !strncmp(line, var(ADDIU), strlen(var(ADDIU)))) {
        if (debug) printf("I_TYPE : %s",line);
        char inst [6];
        int rs, rt;
        int imm;
        sscanf(line, "%s $%d, $%d, %d",inst,&rs,&rt,&imm);
        if (debug) printf("PARSED I_TYPE : %s $%d, $%d, %d\n", inst, rs,rt, imm);
        struct i_type i = {lookup(inst), rs,rt, imm & 0xFFFF};
        memcpy(instrucions+current_instruction, &i, sizeof(i));
        current_instruction ++;
      }
      if (!strncmp(line, var(J), strlen(var(J)))) {
        if (debug) printf("J_TYPE : %s",line);
        char inst [6];
        int imm;
        sscanf(line, "%s 0x%X",inst,&imm);
        if (debug) printf("PARSED J_TYPE : %s 0x%08X\n", inst, imm);
        struct j_type j = {lookup(inst), (uint32_t)(imm & 0x03FFFFFF)};
        memcpy(instrucions+current_instruction, &j, sizeof(j));
        current_instruction ++;
      }
  }

  struct machine m;
  memset(&m, 0, sizeof(struct machine));
  if (debug) {
    for (int i = 0; i < current_instruction; i++) {
      print_instruction(instrucions+i);
    }
  }
  load_program(&m, instrucions, current_instruction);
  struct inst_info info;
  struct inst_info bht_info;
  struct inst_info bhsrt_info;
  int n_branches = 0;
  int bht_mispredicted = 0;
  int bhsrt_mispredicted = 0;
  while(1) {
    uint32_t last_pc = m.pc;
    predict_branch(&m, &bht_info, &bhsrt_info);
    if(step_machine(&m, &info)) {
      break;
    }
    if (info.inst_type != NEITHER) {
      n_branches +=1;
      if (bht_info.taken != info.taken) {
        bht_mispredicted++;
      }
      if (bhsrt_info.taken != info.taken) {
        bhsrt_mispredicted++;
      }
    }
    update_predictor(&m, &info, last_pc);
  }
  printf("%s",desciption);
  printf("--------------------------------------------------------------------------------\n");
  printf("results for %s\n",argv[1]);
  printf("--------------------------------------------------------------------------------\n");
  printf("Single level Branch History Table\n");
  printf("n branches = %d\nn mispredicted = %d\npercentage correct = %f\n",n_branches, bht_mispredicted, ((float)(n_branches - bht_mispredicted)/ n_branches));
  printf("--------------------------------------------------------------------------------\n");
  printf("Two level Branch History Table with Branch History Shift Register Table and Pattern History Table\n");
  printf("n branches = %d\nn mispredicted = %d\npercentage correct = %f\n",n_branches, bhsrt_mispredicted, ((float)(n_branches - bhsrt_mispredicted)/ n_branches));
  printf("--------------------------------------------------------------------------------\n");
}
