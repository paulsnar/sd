var SD = { };

SD.TypeError = function SDTypeError(ip) {
  Error.call(this, 'SD: Type error at ip=' + ip);
}
SD.TypeError.prototype = Object.create(Error.prototype);
SD.TypeError.prototype.constructor = SD.TypeError;

SD.SubroutineError = function SDSubroutineError(ip) {
  Error.call(this, 'SD: Unknown subroutine at ip=' + ip);
}
SD.SubroutineError.prototype = Object.create(Error.prototype);
SD.SubroutineError.prototype.constructor = SD.SubroutineError;

SD.StateError = function SDStateError(ip, message) {
  Error.call(this, 'SD: State error at ip=' + ip + ': ' + message);
}
SD.StateError.prototype = Object.create(Error.prototype);
SD.StateError.prototype.constructor = SD.StateError;

SD.run = function run(code) {
  "use strict";
  code = code.split('');

  var stop = new Object();

  var st = [ ],
      sr = { },
      ip = 0,
      cs = [ ],
      rt = [ ],
      rg = 0;

  var instr;

  function skip() {
    var level = 1;
    while (level > 0) {
      ip += 1;
      instr = code[ip];
      if (instr === undefined) {
        return false;
      }
      if (instr === '{') {
        level += 1;
      } else if (instr === '}') {
        level -= 1;
      }
    }
    return true;
  }

  il: for (;;) {
    instr = code[ip];
    if (instr === undefined) {
      break il;
    }

    if ('0' <= instr && instr <= '9') { // [] -> [literal]
      var val = 9 - (57 - instr.charCodeAt(0));
      st.push(val);
    } else if ('A' <= instr && instr <= 'Z') { // [] -> [literal]
      st.push(instr);
    } else if (instr === '{') { // [] -> [start-ip]
      st.push({ ip: ip });
      if ( ! skip()) {
        break il;
      }
    } else if (instr === '}') { // [] -> []
      ip = cs.pop();
      if (ip === undefined) {
        break il;
      }
    } else if (instr === 'f') { // [start-ip name] -> []
      var name = st.pop(),
          start = st.pop();
      if (typeof name !== 'string' ||
          typeof start !== 'object' ||
          ! ('ip' in start)) {
        throw new SD.TypeError(ip);
      }
      sr[name] = start;
    } else if (instr === 'a' || instr === 's' || instr === 'm' || instr === 'd') { // [a b] -> [c]
      var b = st.pop(),
          a = st.pop();
      var val;
      switch (instr) {
        case 'a': val = a + b; break;
        case 's': val = a - b; break;
        case 'm': val = a * b; break;
        case 'd': val = Math.floor(a / b); break;
      }
      st.push(val);
    } else if (instr === 'j') { // [symbol-or-relative] -> []
      var target = st.pop();
      if (typeof target === 'number') {
        ip += target;
        continue il;
      } else if (typeof target === 'string') {
        target = sr[target];
        if (target === undefined) {
          throw new SD.SubroutineError(ip);
        }
        ip = target.ip;
      } else {
        throw new SD.TypeError(ip);
      }
    } else if (instr === 'c') { // [symbol] -> []
      var target = st.pop();
      if (typeof target !== 'string') {
        throw new SD.TypeError(ip);
      }
      target = sr[target];
      if (target === undefined) {
        throw new SD.SubroutineError(ip);
      }
      cs.push(ip);
      ip = target.ip;
    } else if (instr === 'i') { // [condition true false] -> []
      var branchFalse = st.pop(),
          branchTrue = st.pop(),
          condition = st.pop();
      if (typeof condition !== 'number') {
        throw new SD.TypeError(ip);
      }
      var branch = condition === 0 ? branchFalse : branchTrue;
      if (typeof branch !== 'string') {
        throw new SD.TypeError(ip);
      }
      var target = sr[branch];
      if (target === undefined) {
        throw new SD.SubroutineError(ip);
      }
      cs.push(ip);
      ip = target.ip;
    } else if (instr === 'k') { // [condition true false] -> []
      var branchFalse = st.pop(),
          branchTrue = st.pop(),
          condition = st.pop();
      if (typeof condition !== 'number') {
        throw new SD.TypeError(ip);
      }
      var branch = condition === 0 ? branchFalse : branchTrue;
      if (typeof branch === 'number') {
        ip += branch;
        continue il;
      } else if (typeof branch === 'string') {
        var target = sr[branch];
        if (target === undefined) {
          throw new SD.SubroutineError(ip);
        }
        ip = target.ip;
      } else {
        throw new SD.TypeError(ip);
      }
    } else if (instr === 'r') { // [a] -> [a]
      var val = st[st.length - 1];
      if (typeof val !== 'number') {
        throw new SD.TypeError(ip);
      }
      rt.push(val);
    } else if (instr === 'q') { // [a] -> []
      st.pop();
    } else if (instr === 'w') { // [a] -> [a a]
      st.push(st[st.length - 1]);
    } else if (instr === 'e') { // [a b] -> [b a]
      var a = st.pop(),
          b = st.pop();
      st.push(a, b);
    } else if (instr === 'z') { // [] -> [top-index]
      st.push(st.length);
    } else if (instr === 'x') { // [index] -> [value]
      var index = st.pop();
      if (typeof index !== 'number') {
        throw new SD.TypeError(ip);
      } else if (index > st.length) {
        throw new SD.StateError(ip, 'index higher than stack top');
      }
      st.push(st[index]);
    } else if (instr === 'y') { // [index value] -> []
      var value = st.pop(),
          index = st.pop();
      if (typeof index !== 'number') {
        throw new SD.TypeError(ip);
      } else if (index > st.length) {
        throw new SD.StateError(ip, 'index higher than stack top');
      }
      st[index] = value;
    } else if (instr === 't') { // [a] -> [r]
      var value = st.pop();
      if (typeof value !== 'number') {
        throw new SD.TypeError(ip);
      }
      st.push(rg);
      rg = value;
    } else if (instr === 'h') { // [] -> []
      break il;
    }

    ip += 1;
  }

  return rt;
}

if (global) {
  global.SD = SD;
}
