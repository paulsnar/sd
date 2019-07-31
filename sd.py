# syntax  name                          stack-before                     stack-after
# ==================================================================================
# [0-9]   push integer literal          []                            -> [literal]
# [A-Z]   push symbol literal           []                            -> [literal]
# {.*}    subroutine (see below)        []                            -> [start-ip]
# a       add                           [a b]                         -> [a+b]
# c       call                          [name]                        -> []
# d       divide                        [a b]                         -> [a/b]
# e       exchange (swap)               [a b]                         -> [b a]
# f       define subroutine             [start-ip name]               -> []
# h       halt                          []                            -> []
# i       conditional call              [condition if-true if-false]  -> []
# j       jump to subroutine            [symbol-or-relative]          -> []
# k       conditional jump              [condition if-true if-false]  -> []
# m       multiply                      [a b]                         -> [a*b]
# q       pop (discard)                 [a]                           -> []
# r       append stack top to result    [a]                           -> [a]
# s       subtract                      [a b]                         -> [a-b]
# t       swap with register            [a]                           -> [r]
# w       duplicate                     [a]                           -> [a a]
# x       index into stack              [index]                       -> [value]
# y       set stack value               [index value]                 -> []
# z       push stack length             []                            -> [top-index]

def run(code):
  code = list(code)

  stack = list()
  subroutines = dict()
  instructionpointer = 0
  callstack = list()
  result = list()
  register = 0

  def push_val(val):
    nonlocal stack
    stack.append(val)

  def pop_val():
    nonlocal stack
    return stack.pop()

  def skip():
    nonlocal code, instructionpointer
    level = 1
    while level > 0:
      instructionpointer += 1
      try:
        if code[instructionpointer] == '{':
          level += 1
        elif code[instructionpointer] == '}':
          level -= 1
      except IndexError:
        instructionpointer = len(code)

  while True:
    try:
      instruction = code[instructionpointer]
    except IndexError:
      break

    # push integer literal [] -> [literal]
    if '0' <= instruction <= '9':
      val = 9 - (57 - ord(instruction))
      push_val(val)

    # push symbol literal [] -> [literal]
    elif 'A' <= instruction <= 'Z':
      push_val(instruction)

    # subroutine [] -> [start-ip]
    elif instruction == '{':
      push_val(instructionpointer)
      ip_pre = instructionpointer
      skip()

    # return [] -> []
    elif instruction == '}':
      try:
        instructionpointer = callstack.pop()
      except IndexError:
        break

    # define subroutine [start-ip name] -> []
    elif instruction == 'f':
      name = pop_val()
      start = pop_val()
      subroutines[name] = start

    # add [a b] -> [a+b]
    elif instruction == 'a':
      b = pop_val()
      a = pop_val()
      push_val(a + b)

    # subtract [a b] -> [a-b]
    elif instruction == 's':
      b = pop_val()
      a = pop_val()
      push_val(a - b)

    # multiply [a b] -> [a*b]
    elif instruction == 'm':
      b = pop_val()
      a = pop_val()
      push_val(a * b)

    # divide [a b] -> [a/b]
    elif instruction == 'd':
      b = pop_val()
      a = pop_val()
      push_val(a // b)

    # jump [symbol-or-relative] -> []
    elif instruction == 'j':
      target = pop_val()
      if isinstance(target, str):
        instructionpointer = subroutines[target]
      else:
        instructionpointer += target
        continue

    # call subroutine [symbol] -> []
    elif instruction == 'c':
      callstack.append(instructionpointer)
      name = pop_val()
      instructionpointer = subroutines[name]

    # conditional call [condition if-true if-false] -> []
    elif instruction == 'i':
      if_false = pop_val()
      if_true = pop_val()
      cond = pop_val()
      if cond == 0:
        branch = if_false
      else:
        branch = if_true
      callstack.append(instructionpointer)
      instructionpointer = subroutines[branch]

    # conditional jump [condition if-true if-false] -> []
    elif instruction == 'k':
      if_false = pop_val()
      if_true = pop_val()
      cond = pop_val()
      if cond == 0:
        branch = if_false
      else:
        branch = if_true
      if isinstance(branch, str):
        instructionpointer = subroutines[branch]
      else:
        instructionpointer += branch
        continue

    # append top to result [a] -> [a]
    elif instruction == 'r':
      result.append(stack[-1])

    # discard [a] -> []
    elif instruction == 'q':
      pop_val()

    # duplicate [a] -> [a a]
    elif instruction == 'w':
      push_val(stack[-1])

    # exchange [a b] -> [b a]
    elif instruction == 'e':
      a = pop_val()
      b = pop_val()
      push_val(a)
      push_val(b)

    # stack top [] -> [top-index]
    elif instruction == 'z':
      push_val(len(stack))

    # index into stack [index] -> [value]
    elif instruction == 'x':
      index = pop_val()
      push_val(stack[index])

    # set stack index [index value] -> []
    elif instruction == 'y':
      value = pop_val()
      index = pop_val()
      stack[index] = value

    # swap with register [a] -> [r]
    elif instruction == 't':
      val = pop_val()
      register, val = val, register
      push_val(val)

    # halt [] -> []
    elif instruction == 'h':
      break

    instructionpointer += 1

  return bytes(result)

if __name__ == '__main__':
  import sys
  try:
    program = sys.argv[1]
    with open(program, 'r') as f:
      program = f.read()
  except IndexError:
    program = sys.stdin.read()

  if program == '':
    print(f'Usage: {sys.argv[0]} program\n  or pass the program via stdin')
    sys.exit(1)

  print(run(program))
