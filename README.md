# sd

Single-letter stack-based language for simple stuff.

Seems Turing-complete, but don't quote me on that.

## Language

The language itself consists of single letter "commands" which are executed in
order. The memory model is similar to WebAssembly, i.e., there's no
homoiconity.

All the commands are listed below:

     syntax | name                  | stack before      | stack after
    --------+-----------------------+-------------------+-------------
      [0-9] | push integer literal  | []                | [literal]
      [A-Z] | push symbol literal   | []                | [literal]
      {.*}  | subroutine            | []                | [start-ip]
      a     | add                   | [a b]             | [a+b]
      c     | call                  | [target]          | []
      d     | divide                | [a b]             | [a/b]
      e     | exchange              | [a b]             | [b a]
      f     | define subroutine     | [start-ip name]   | []
      h     | halt                  | []                | []
      i     | conditional call      | [cond true false] | []
      j     | jump                  | [target]          | []
      k     | conditional jump      | [cond true false] | []
      m     | multiply              | [a b]             | [a*b]
      q     | discard               | [a]               | []
      r     | append to result      | [a]               | [a]
      s     | subtract              | [a b]             | [a-b]
      t     | swap with register    | [a]               | [r]
      w     | duplicate             | [a]               | [a a]
      x     | stack index get       | [index]           | [value]
      y     | stack index set       | [index value]     | []
      z     | stack top index       | []                | [top-index]

    h   halt  :: []   -> []
      The simplest opcode -- halts the execution.

    [0-9]   push integer literal :: [] -> [literal]
      Pushes the literal integer value 0-9.

    [A-Z]   push symbol literal :: [] -> [literal]
      Pushes the literal symbol value A-Z.

    {.*}    subroutine :: [] -> [start-ip]
      The opening brace pushes its Instruction Pointer then skips instructions
      up to and including the matching `}`, with support for nesting. This is
      intended for use with `f`.

    f   define subroutine :: [start-ip name]    -> []
      Stores the value of `start-ip` within the subroutine dictionary under the
      name of `name`. The named subroutine can then be called with `c` or `i`,
      or jumped into by `j` or `k`.

    j   jump              :: [target]           -> []
      Jump to the specified subroutine's start (if target is a symbol) or
      add the value to the instruction pointer (perform a relative jump).
      If jumping to a subroutine, reaching its end (`}`) without further
      control transfer will cause the VM to halt.

    k   conditional jump  :: [cond true false]  -> []
      Perform a jump (same as `j`), but choose the target depending on `cond`:
      if it's 0, then choose the `false` branch; otherwise choose the `true`
      branch. As with `j`, the branch values can either be symbols for
      subroutines or integers for relative jumps. (The address is relative to
      the `k` command.)

    c   call              :: [target]           -> []
    i   conditional call  :: [cond true false]  -> []
      Similar to `j` and `k`, except the target can only be a symbol, and the
      control flow will continue with the following instruction when the
      subroutine ends.

    e   exchange  :: [a b]  -> [b a]
    q   discard   :: [a]    -> []
    w   duplicate :: [a]    -> [a a]
      Standard stack operations. Work as you'd expect.

    a   add       :: [a b]  -> [a+b]
    d   divide    :: [a b]  -> [a/b]
    m   multiply  :: [a b]  -> [a*b]
    s   subtract  :: [a b]  -> [a-b]
      Standard arithmetic operations. Operands and results are integers.

    r   append to result  :: [a]  -> [a]
      Appends the stack top to the result list. The value of the result list
      will be returned to the caller when the VM halts.
      The stack top remains unchanged.

    t   swap with register  :: [a]  -> [r]
      Swaps the stack top with the register. The initial value of the register
      is 0.

    x   stack index get :: [index]        -> [value]
    y   stack index set :: [index value]  -> []
      Allows for manipulating the stack as an array. Indexes are zero-based.
      Accessing indexes outside of the bounds set by pushing/popping, as well
      as negative indexes, will cause the VM to stop with an error.

    z   stack top index :: [] -> [top-index]
      Pushes the stack top index as it is before this instruction is executed.
      If the stack is empty, 0 will be pushed (i.e., after this instruction the
      stack index will be 1 higher than it reports).

## Environment

Right now the VM has implementations in Python and JS, which means that the
runtime is technically duck-typed and types are not really enforced. These
behaviours should not be relied upon.

The stack size is currently dynamic, and the stack is heterogenous in nature.

The VM is implemented as a function which returns the value of the result list
at the time of VM halting, unless an error occurs, in which case an exception
is thrown.

## Examples

    Example: return b'hello'

    55m4m4ar3sr7arr3arqh
    55m                   push 5, 5 then multiply   [25]
       4m                 * 4                       [100]
         4a               + 4                       [104]
           r              append to return
            3sr           - 3, append to return     [101]
               7a         + 7                       [108]
                 rr       append to return twice
                   3ar    + 3, append to return     [111]
                      qh  pop and halt              []


    Example: conditionals

    {1r}Af{0r}Bf4ABiqh
    {1r}Af              define subroutine A, which
     1r                   appends 1 to result
          {0r}Bf        define subroutine B, which
           0r             appends 0 to result
                4       pushes 4
                 ABi      conditional call: true -> A, false -> B
                          ; results in call to A
                    qh  pop 1 or 0, then halt
                          ; result => [1]

Note that these examples leave the stack empty; there's no such requirement,
but it's a good practice anyway.

## License

[ISC](./LICENSE.txt)
