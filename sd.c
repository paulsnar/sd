#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum sd_status {
  STAT_OK,
  STAT_NOOP,
  STAT_HALT,
  STAT_ERROVERFLOW,
  STAT_ERRMEM,
  STAT_ERRSTATE,
  STAT_ERRTYPE,
  STAT_ERRSUBROUTINE
} sd_status;

typedef int64_t sd_integer_t;
typedef unsigned char sd_symbol_t;
typedef int64_t sd_ip_t;

typedef union sd_value {
  sd_integer_t integer;
  sd_symbol_t symbol;
  sd_ip_t ip;
} sd_value;

enum sd_value_type {
  VAL_INTEGER,
  VAL_SYMBOL,
  VAL_IP
};

struct sd_item {
  enum sd_value_type t;
  sd_value v;
};

struct sd_item_maybe {
  struct sd_item val;
  uint8_t is_present;
};

struct sd_stack_entry {
  struct sd_item val;
  struct sd_stack_entry *next, *prev;
};

struct sd_stack {
  unsigned int size;
  struct sd_stack_entry *top, *bottom;
};

static void sd_stack_init(struct sd_stack *stack)
{
  stack->size = 0;
  stack->top = stack->bottom = NULL;
}

static sd_status sd_stack_push(
  struct sd_stack *stack, enum sd_value_type t, sd_value val)
{
  struct sd_stack_entry *ent = malloc(sizeof(struct sd_stack_entry));
  if (ent == NULL) {
    return STAT_ERRMEM;
  }

  ent->val.t = t;
  ent->val.v = val;
  ent->prev = stack->top;
  ent->next = NULL;

  if (stack->top == NULL) {
    stack->top = stack->bottom = ent;
  } else {
    stack->top->next = ent;
    stack->top = ent;
  }
  stack->size += 1;

  if (stack->size == 0) {
    return STAT_ERROVERFLOW;
  }

  return STAT_OK;
}

static struct sd_item_maybe sd_stack_pop(struct sd_stack *stack)
{
  struct sd_item_maybe val;
  if (stack->top == NULL) {
    val.is_present = 0;
    return val;
  }
  struct sd_stack_entry *item = stack->top;
  if (item->prev == NULL) {
    stack->top = stack->bottom = NULL;
  } else {
    stack->top = item->prev;
    stack->top->next = NULL;
  }
  stack->size -= 1;

  val.is_present = 1;
  val.val.t = item->val.t;
  val.val.v = item->val.v;
  free(item);
  return val;
}

static struct sd_item_maybe sd_stack_peek(struct sd_stack *stack)
{
  struct sd_item_maybe val;
  if (stack->top == NULL) {
    val.is_present = 0;
  } else {
    val.is_present = 1;
    val.val.t = stack->top->val.t;
    val.val.v = stack->top->val.v;
  }
  return val;
}

static sd_status sd_stack_duplicate(struct sd_stack *stack)
{
  if (stack->top == NULL) {
    return STAT_NOOP;
  }
  return sd_stack_push(stack, stack->top->val.t, stack->top->val.v);
}

static sd_status sd_stack_duplicate_at(
  struct sd_stack *stack, sd_integer_t index)
{
  if (index > stack->size) {
    return STAT_ERRSTATE;
  }
  struct sd_stack_entry *ent = stack->bottom;
  for (unsigned int i = 0; i != index; i += 1) {
    ent = ent->next;
    if (ent == NULL) {
      return STAT_ERRSTATE;
    }
  }

  return sd_stack_push(stack, ent->val.t, ent->val.v);
}

static sd_status sd_stack_replace_at(
  struct sd_stack *stack, sd_integer_t index, enum sd_value_type t, sd_value v)
{
  if (index > stack->size) {
    return STAT_ERRSTATE;
  }
  struct sd_stack_entry *ent = stack->bottom;
  for (unsigned int i = 0; i != index; i += 1) {
    ent = ent->next;
    if (ent == NULL) {
      return STAT_ERRSTATE;
    }
  }

  ent->val.t = t;
  ent->val.v = v;
  return STAT_OK;
}

struct sd_subr_table {
  sd_ip_t subroutines[26];
  uint32_t has_subroutines_bitfield;
};

static void sd_subr_table_init(struct sd_subr_table *table)
{
  table->has_subroutines_bitfield = 0;
}

static sd_status sd_subr_table_set(struct sd_subr_table *table,
  sd_symbol_t symbol, sd_ip_t ip)
{
  unsigned char index = symbol - 'A';
  table->subroutines[index] = ip;
  table->has_subroutines_bitfield =
    table->has_subroutines_bitfield | (1 << index);
  return STAT_OK;
}

static sd_ip_t sd_subr_table_get(struct sd_subr_table *table, sd_symbol_t symbol)
{
  unsigned char index = symbol - 'A';
  if (table->has_subroutines_bitfield & (1 << index)) {
    return table->subroutines[index];
  } else {
    return -1;
  }
}

struct sd_call_stack_entry {
  sd_ip_t ip;
  struct sd_call_stack_entry *next, *prev;
};

struct sd_call_stack {
  struct sd_call_stack_entry *top, *bottom;
};

static void sd_call_stack_init(struct sd_call_stack *stack)
{
  stack->top = stack->bottom = NULL;
}

static sd_status sd_call_stack_push(struct sd_call_stack *stack, sd_ip_t ip)
{
  struct sd_call_stack_entry *entry =
    malloc(sizeof(struct sd_call_stack_entry));
  if (entry == NULL) {
    return STAT_ERRMEM;
  }

  entry->ip = ip;
  entry->next = NULL;

  if (stack->top == NULL) {
    stack->top = stack->bottom = entry;
    entry->prev = NULL;
  } else {
    entry->prev = stack->top;
    stack->top->next = entry;
    stack->top = entry;
  }

  return STAT_OK;
}

static sd_ip_t sd_call_stack_pop(struct sd_call_stack *stack)
{
  if (stack->top == NULL) {
    return -1;
  }

  struct sd_call_stack_entry *entry = stack->top;
  sd_ip_t ip = entry->ip;

  stack->top = entry->prev;
  if (stack->top == NULL) {
    stack->bottom = NULL;
  } else {
    stack->top->next = NULL;
  }
  free(entry);

  return ip;
}

struct sd_result {
  size_t len, cap;
  sd_integer_t *out;
};

static void sd_result_init(struct sd_result *result)
{
  result->len = 0;
  result->cap = 0;
  result->out = NULL;
}

static sd_status sd_result_grow(struct sd_result *result)
{
  if (result->cap == 0) {
    result->cap = 256;
  } else {
    result->cap *= result->cap * 2;
  }

  result->out = realloc(result->out, result->cap * sizeof(sd_integer_t));

  if (result->out == NULL) {
    result->cap = -1;
    return STAT_ERRMEM;
  }

  return STAT_OK;
}

static sd_status sd_result_append(struct sd_result *result, sd_integer_t val)
{
  if (result->len == result->cap) {
    sd_status stat = sd_result_grow(result);
    if (stat != STAT_OK) {
      return stat;
    }
  }

  result->out[result->len] = val;
  result->len += 1;

  return STAT_OK;
}

static void sd_result_print_i8(struct sd_result *result)
{
  int8_t *r = malloc(sizeof(int8_t) * result->len);
  if (r == NULL) {
    // okay let's bitbang then
    for (size_t i = 0, l = result->len; i < l; i += 1) {
      fwrite(result->out + i, sizeof(int8_t), 1, stdout);
    }
  }

  for (size_t i = 0, l = result->len; i < l; i += 1) {
    r[i] = (int8_t) result->out[i];
  }

  fwrite(r, sizeof(int8_t), result->len, stdout);
  free(r);
}

static void sd_result_print_i16(struct sd_result *result)
{
  int16_t *r = malloc(sizeof(int16_t) * result->len);
  if (r == NULL) {
    for (size_t i = 0, l = result->len; i < l; i += 1) {
      fwrite(result->out + i, sizeof(int16_t), 1, stdout);
    }
  }

  for (size_t i = 0, l = result->len; i < l; i += 1) {
    r[i] = (int16_t) result->out[i];
  }

  fwrite(r, sizeof(int16_t), result->len, stdout);
  free(r);
}

static void sd_result_print(struct sd_result *result)
{
  bool can_i8 = true, can_i16 = true;

  for (size_t i = 0, l = result->len; i < l; i += 1) {
    sd_integer_t value = result->out[i];
    if (-128 <= value && value < 128) {
      // can_i8 = true
    } else if (-32768 <= value && value < 32768) {
      can_i8 = false;
      // can_i16 = true
    } else if (value < -32768 || 32768 <= value) {
      can_i8 = false;
      can_i16 = false;
      break;
    }
  }

  if (can_i8) {
    sd_result_print_i8(result);
  } else if (can_i16) {
    sd_result_print_i16(result);
  } else {
    fwrite(result->out, sizeof(sd_integer_t), result->len, stdout);
  }
}

struct sd_vm_state {
  struct sd_stack *stack;
  struct sd_subr_table *subroutines;
  struct sd_call_stack *call_stack;
  struct sd_result *result;

  char *code;
  size_t code_len;
  sd_ip_t instruction_pointer;
  sd_integer_t register_1;
};

static sd_status sd_vm_init(struct sd_vm_state *state)
{
  state->stack = malloc(sizeof(struct sd_stack));
  if (state->stack == NULL) {
    return STAT_ERRMEM;
  }
  sd_stack_init(state->stack);

  state->subroutines = malloc(sizeof(struct sd_subr_table));
  if (state->subroutines == NULL) {
    return STAT_ERRMEM;
  }
  sd_subr_table_init(state->subroutines);

  state->call_stack = malloc(sizeof(struct sd_call_stack));
  if (state->call_stack == NULL) {
    return STAT_ERRMEM;
  }
  sd_call_stack_init(state->call_stack);

  state->result = malloc(sizeof(struct sd_result));
  if (state->result == NULL) {
    return STAT_ERRMEM;
  }
  sd_result_init(state->result);

  state->code = NULL;
  state->code_len = 0;
  state->instruction_pointer = 0;
  state->register_1 = 0;

  return STAT_OK;
}

static void sd_vm_deinit(struct sd_vm_state *state)
{
  // sd_result_deinit(state->result);
  // sd_call_stack_deinit(state->call_stack);
  // sd_subr_table_deinit(state->subr_table);
  // sd_stack_deinit(state->stack);
}

static sd_status sd_run_skip(struct sd_vm_state *state)
{
  char instruction;
  int level = 1;
  while (level > 0) {
    state->instruction_pointer += 1;
    if (state->instruction_pointer > state->code_len) {
      return STAT_HALT;
    }
    instruction = state->code[state->instruction_pointer];
    if (instruction == '{') {
      level += 1;
    } else if (instruction == '}') {
      level -= 1;
    }
  }
  return STAT_OK;
}

#define SD_TRY(act) do { \
    stat = (act); \
    if (stat != STAT_OK) { goto break_intloop; } \
  } while (0)

#define SD_TRY_POP_ANY() do { \
    item_maybe = sd_stack_pop(state.stack); \
    if ( ! item_maybe.is_present) { \
      stat = STAT_ERRSTATE; goto break_intloop; } } while (0)

#define SD_TRY_POP_ITEM(type) do { \
    item_maybe = sd_stack_pop(state.stack); \
    if ( ! item_maybe.is_present) { \
      stat = STAT_ERRSTATE; goto break_intloop; \
    } else if (item_maybe.val.t != (type)) { \
      stat = STAT_ERRTYPE; goto break_intloop; \
    } } while (0)

#define SD_TRY_JUMP_SUBROUTINE(name) do { \
    sd_ip_t target = sd_subr_table_get(state.subroutines, name); \
    if (target == -1) { \
      stat = STAT_ERRSUBROUTINE; goto break_intloop; } \
    state.instruction_pointer = target; \
  } while (0)

#define SD_TRY_ARITHMETIC(expression) do { \
    SD_TRY_POP_ITEM(VAL_INTEGER); sd_integer_t b = item_maybe.val.v.integer; \
    SD_TRY_POP_ITEM(VAL_INTEGER); sd_integer_t a = item_maybe.val.v.integer; \
    SD_TRY(sd_stack_push(state.stack, VAL_INTEGER, (sd_value) (expression))); \
  } while (0)

static sd_status sd_run(char *source, size_t source_len)
{
  struct sd_vm_state state;
  sd_vm_init(&state);

  state.code = source;
  state.code_len = source_len;

  char instruction;
  sd_status stat;
  struct sd_item_maybe item_maybe;

  for (;;) {
    if (state.instruction_pointer > source_len) {
      break;
    }

    instruction = source[state.instruction_pointer];

    if ('0' <= instruction && instruction <= '9') {
      sd_integer_t val = 9 - (57 - instruction);
      SD_TRY(sd_stack_push(state.stack, VAL_INTEGER, (sd_value) val));
      goto skip_intswitch;
    } else if ('A' <= instruction && instruction <= 'Z') {
      sd_symbol_t val = instruction;
      SD_TRY(sd_stack_push(state.stack, VAL_SYMBOL, (sd_value) val));
      goto skip_intswitch;
    }

    switch (instruction) {
      case '{': { // start subroutine [] -> [start-ip]
        SD_TRY(sd_stack_push(state.stack, VAL_IP,
          (sd_value) state.instruction_pointer));
        SD_TRY(sd_run_skip(&state));
        break;
      }

      case '}': { // return [] -> []
        state.instruction_pointer = sd_call_stack_pop(state.call_stack);
        if (state.instruction_pointer == -1) {
          goto break_intloop;
        }
        break;
      }

      case 'f': { // define function [start-ip name] -> []
        SD_TRY_POP_ITEM(VAL_SYMBOL);
        sd_symbol_t name = item_maybe.val.v.symbol;
        SD_TRY_POP_ITEM(VAL_IP);
        sd_ip_t ip = item_maybe.val.v.ip;
        SD_TRY(sd_subr_table_set(state.subroutines, name, ip));
        break;
      }

      case 'a': { // add [a b] -> [a+b]
        SD_TRY_ARITHMETIC(a + b);
        break;
      }

      case 's': { // subtract [a b] -> [a-b]
        SD_TRY_ARITHMETIC(a - b);
        break;
      }

      case 'm': { // multiply [a b] -> [a*b]
        SD_TRY_ARITHMETIC(a * b);
        break;
      }

      case 'd': { // divide [a b] -> [a/b]
        SD_TRY_ARITHMETIC(a / b);
        break;
      }

      case 'j': { // jump [symbol-or-relative] -> []
        SD_TRY_POP_ANY();
        if (item_maybe.val.t == VAL_INTEGER) {
          sd_integer_t delta = item_maybe.val.v.integer;
          state.instruction_pointer += delta;
          goto cont_intloop;
        } else if (item_maybe.val.t == VAL_SYMBOL) {
          sd_symbol_t name = item_maybe.val.v.symbol;
          SD_TRY_JUMP_SUBROUTINE(name);
        } else {
          stat = STAT_ERRTYPE;
          goto break_intloop;
        }
        break;
      }

      case 'c': { // call subroutine [symbol] -> []
        SD_TRY(sd_call_stack_push(
          state.call_stack, state.instruction_pointer));
        SD_TRY_POP_ITEM(VAL_SYMBOL);
        sd_symbol_t name = item_maybe.val.v.symbol;
        SD_TRY_JUMP_SUBROUTINE(name);
        break;
      }

      case 'i': { // conditional call [condition true false] -> []
        SD_TRY_POP_ITEM(VAL_SYMBOL);
        sd_symbol_t branch_false = item_maybe.val.v.symbol;
        SD_TRY_POP_ITEM(VAL_SYMBOL);
        sd_symbol_t branch_true = item_maybe.val.v.symbol;
        SD_TRY_POP_ITEM(VAL_INTEGER);
        sd_integer_t condition = item_maybe.val.v.integer;
        sd_symbol_t branch = condition == 0 ? branch_false : branch_true;

        SD_TRY(sd_call_stack_push(
          state.call_stack, state.instruction_pointer));
        SD_TRY_JUMP_SUBROUTINE(branch);
        break;
      }

      case 'k': { // conditional jump [condition true false] -> []
        struct sd_item branch, branch_true, branch_false;
        SD_TRY_POP_ANY();
        branch_false = item_maybe.val;
        SD_TRY_POP_ANY();
        branch_true = item_maybe.val;
        SD_TRY_POP_ITEM(VAL_INTEGER);
        sd_integer_t condition = item_maybe.val.v.integer;
        branch = condition == 0 ? branch_false : branch_true;

        if (branch.t == VAL_SYMBOL) {
          SD_TRY_JUMP_SUBROUTINE(branch.v.symbol);
        } else if (branch.t == VAL_INTEGER) {
          state.instruction_pointer += branch.v.integer;
          goto cont_intloop;
        } else {
          stat = STAT_ERRTYPE;
          goto break_intloop;
        }
        break;
      }

      case 'r': { // append to result [a] -> [a]
        item_maybe = sd_stack_peek(state.stack);
        if (item_maybe.is_present == 0) {
          stat = STAT_ERRSTATE;
          goto break_intloop;
        } else if (item_maybe.val.t != VAL_INTEGER) {
          stat = STAT_ERRTYPE;
          goto break_intloop;
        } else {
          SD_TRY(sd_result_append(state.result, item_maybe.val.v.integer));
        }
        break;
      }

      case 'q': { // discard [a] -> []
        (void) sd_stack_pop(state.stack);
        break;
      }

      case 'w': { // duplicate [a] -> [a a]
        (void) sd_stack_duplicate(state.stack);
        break;
      }

      case 'e': { // exchange [a b] -> [b a]
        struct sd_item a, b;
        SD_TRY_POP_ANY();
        a = item_maybe.val;
        SD_TRY_POP_ANY();
        b = item_maybe.val;
        SD_TRY(sd_stack_push(state.stack, a.t, a.v));
        SD_TRY(sd_stack_push(state.stack, b.t, b.v));
        break;
      }

      case 'z': { // stack top [] -> [top-index]
        sd_integer_t top = (sd_integer_t) state.stack->size;
        SD_TRY(sd_stack_push(state.stack, VAL_INTEGER, (sd_value) top));
        break;
      }

      case 'x': { // index into stack [index] -> [value]
        SD_TRY_POP_ITEM(VAL_INTEGER);
        sd_integer_t index = item_maybe.val.v.integer;
        SD_TRY(sd_stack_duplicate_at(state.stack, index));
        break;
      }

      case 'y': { // set stack index [index value] -> []
        SD_TRY_POP_ANY();
        struct sd_item value = item_maybe.val;
        SD_TRY_POP_ITEM(VAL_INTEGER);
        sd_integer_t index = item_maybe.val.v.integer;
        SD_TRY(sd_stack_replace_at(state.stack, index, value.t, value.v));
        break;
      }

      case 't': { // swap with register [a] -> [r]
        SD_TRY_POP_ITEM(VAL_INTEGER);
        sd_integer_t val = item_maybe.val.v.integer, tmp = state.register_1;
        state.register_1 = val;
        SD_TRY(sd_stack_push(state.stack, VAL_INTEGER, (sd_value) tmp));
        break;
      }

      case 'h': { // halt [] -> []
        stat = STAT_HALT;
        goto break_intloop;
      }
    }

  skip_intswitch:
    state.instruction_pointer += 1;

  cont_intloop: continue;
  break_intloop: break;
  }

  if (stat != STAT_OK && stat != STAT_HALT) {
    switch (stat) {
      case STAT_ERRMEM:
        printf("halted: out of memory (at ip = %li)\n",
          state.instruction_pointer);
        break;

      case STAT_ERRSTATE:
        printf("halted: bad state (at ip = %li)\n",
          state.instruction_pointer);
        break;

      case STAT_ERRTYPE:
        printf("halted: type error (at ip = %li)\n",
          state.instruction_pointer);
        break;

      case STAT_ERRSUBROUTINE:
        printf("halted: call to nonexistant subroutine (at ip = %li)\n",
          state.instruction_pointer);
        break;

      case STAT_ERROVERFLOW:
        printf("halted: stack overflow (at ip = %li)\n",
          state.instruction_pointer);
        break;

      case STAT_OK:
      case STAT_NOOP:
      case STAT_HALT:
        break;
    }
  }

  sd_result_print(state.result);

  sd_vm_deinit(&state);

  return STAT_OK;
}

int main(int argc, char **argv)
{
  if (argc == 1) {
    printf("Usage: %s program\n", argv[0]);
    return 1;
  }

  FILE *input;
  if (strncmp("-", argv[1], 2) == 0) {
    input = stdin;
  } else {
    input = fopen(argv[1], "r");
  }
  if (input == NULL) {
    printf("error: could not open input file: %s\n",
      strerror(errno));
    return 2;
  }

  long size;
  if (input == stdin) {
    size = 16 * 1024;
  } else {
    fseek(input, 0, SEEK_END);
    size = ftell(input);
    rewind(input);
  }
  char *progbuf = malloc(size);

  if (progbuf == NULL) {
    printf("error: could not allocate memory\n");
    return 3;
  }

  size_t offset = 0;
  do {
    size_t len = fread(progbuf + offset, 1, size - offset + 1, input);
    offset += len;
  } while ( ! feof(input) && ! ferror(input));

  if ( ! feof(input) && ferror(input)) {
    int err = ferror(input);
    printf("error: could not read input file: %s\n", strerror(err));
    return 4;
  }

  sd_run(progbuf, offset);

  return 0;
}
