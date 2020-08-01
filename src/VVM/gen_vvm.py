#!/usr/bin/env python

# Generate VVM -- produces myriad headers from the builtin types and opcodes
#
# Copyright (C) 2019--2020 Empirical Software Solutions, LLC
#
# This program is distributed under the terms of the GNU Affero General
# Public License with the Commons Clause.

import sys
import os

#   Empirical,   VVM,   C++
types = [
  ('Int64',     'i64', 'int64_t'),
  ('Float64',   'f64', 'double'),
  ('Bool',      'b8',  'bool'),
  ('String',    'S',   'std::string'),
  ('Char',      'c8',  'char'),
  ('Timestamp', 'T',   'Timestamp'),
  ('Timedelta', 'D',   'Timedelta'),
  ('Time',      'TI',  'Time'),
  ('Date',      'DA',  'Date'),
]

# list of types we will build on
integer_types = ['Int64']
float_types = ['Float64']
bool_types = ['Bool']
string_types = ['String']
char_types = ['Char']
timestamp_types = ['Timestamp']
timedelta_types = ['Timedelta']
time_types = ['Time']
date_types = ['Date']
arithmetic_types = integer_types + float_types
time_ish_types = timestamp_types + time_types + date_types
all_types = (arithmetic_types + bool_types + string_types + char_types +
             time_ish_types + timedelta_types)


def _make_opcodes():
    """ Programmatically build the table of VVM operators """
    # Emprical name, VVM name, Emprical type, opcode arity
    opcodes = [
      ('', 'halt',         '', 0),
      # ()
      ('', 'alloc',        '', 2),
      # Kind->Value
      ('', 'write',        '', 1),
      # String->()
      ('', 'save',         '', 1),
      # String->()
      ('', 'member',       '', 3),
      # ([Value],Int64)->Value
      ('', 'assign',       '', 3),
      # (Value,Kind)->Value
      ('', 'append',       '', 3),
      # (Value,Kind,Value)
      ('', 'repr',         '', 3),
      # (Value,Kind)->String
      ('', 'load',         '', 3),
      # (String,Kind)->Value
      ('', 'store',        '', 4),
      # (Kind,Value,String)->()
      ('', 'where',        '', 4),
      # (Value,[Bool],Kind)->Value
      ('', 'br',           '', 1),
      # Label
      ('', 'btrue',        '', 2),
      # (Bool,Label)
      ('', 'bfalse',       '', 2),
      # (Bool,Label)
      ('', 'ret',          '', 1),
      # (Value,...)
      ('', 'call',         '', 2),
      # (Func,Int64,...)
      ('', 'isort',        '', 3),
      # (Value,Kind)->[Int64]
      ('', 'multidx',      '', 4),
      # (Value,[Int64],Kind)->Value
      ('', 'group',        '', 8),
      # (Kind,Value,Kind,Value,Kind)->(Value,Value,Int64)
      ('', 'eqmatch',      '', 5),
      # (Kind,Value,Value)->([Int64],[Int64])
      ('', 'asofmatch',    '', 7),
      # (Kind,Value,Value,Bool,Int64)->([Int64],[Int64])
      ('', 'asofnear',     '', 7),
      # (Kind,Value,Value,Bool,Int64)->([Int64],[Int64])
      ('', 'asofwithin',   '', 8),
      # (Kind,Value,Value,Bool,Int64,Value)->([Int64],[Int64])
      ('', 'eqasofmatch',  '', 10),
      # (Kind,Value,Value,Kind,Value,Value,Bool,Int64)->([Int64],[Int64])
      ('', 'eqasofnear',   '', 10),
      # (Kind,Value,Value,Kind,Value,Value,Bool,Int64)->([Int64],[Int64])
      ('', 'eqasofwithin', '', 11),
      # (Kind,Value,Value,Kind,Value,Value,Bool,Int64,Value)->([Int64],[Int64])
      ('', 'take',         '', 4),
      # (Kind,Kind,Value)->Value
      ('', 'concat',       '', 4),
      # (Kind,Value,Value)->Value
    ]

    opcodes += [('now', 'now', 'Timestamp', 1)]

    # casting operators
    pairs = [(string_types, all_types),
             (integer_types, all_types),
             (float_types, float_types + integer_types + string_types),
             (char_types, char_types + integer_types),
             (bool_types, bool_types),
             (timestamp_types, time_ish_types + integer_types + string_types),
             (timedelta_types, timedelta_types + integer_types + string_types),
             (time_types, timestamp_types + time_types +
                          integer_types + string_types),
             (date_types, timestamp_types + date_types +
                          integer_types + string_types)]
    patterns = ['%s->%s', '[%s]->[%s]']
    for tgts, srcs in pairs:
        for tgt in tgts:
            for src in srcs:
                for p in patterns:
                    opcodes += [(tgt, 'cast', p % (src, tgt), 2)]

    # print function
    patterns = ['%s->()', '[%s]->()']
    for p in patterns:
        for t in all_types:
            opcodes += [('print', 'print', p % t, 2)]

    # binary operators -- boolean
    operators = [('or', 'or'), ('and', 'and')]
    patterns = ['(%s,%s)->%s',     '(%s,[%s])->[%s]',
                '([%s],%s)->[%s]', '([%s],[%s])->[%s]']
    for k, v in operators:
        for p in patterns:
            for t in bool_types:
                opcodes += [(v, k, p % (t, t, t), 3)]

    # binary operators -- integer
    operators = [('bitand', '&'), ('bitor', '|'), ('lshift', '<<'),
                 ('rshift', '>>'), ('mod', '%')]
    patterns = ['(%s,%s)->%s',     '(%s,[%s])->[%s]',
                '([%s],%s)->[%s]', '([%s],[%s])->[%s]']
    for k, v in operators:
        for p in patterns:
            for t in integer_types:
                opcodes += [(v, k, p % (t, t, t), 3)]

    # binary operators -- arithmetic
    operators = [('add', '+'), ('sub', '-'), ('mul', '*'), ('div', '/')]
    patterns = ['(%s,%s)->%s',     '(%s,[%s])->[%s]',
                '([%s],%s)->[%s]', '([%s],[%s])->[%s]']
    for k, v in operators:
        for p in patterns:
            for t in arithmetic_types:
                opcodes += [(v, k, p % (t, t, t), 3)]

    # binary operators -- comparison
    operators = [('lt', '<'), ('gt', '>'), ('eq', '=='), ('ne', '!='),
                 ('lte', '<='), ('gte', '>=')]
    patterns = ['(%s,%s)->Bool',     '(%s,[%s])->[Bool]',
                '([%s],%s)->[Bool]', '([%s],[%s])->[Bool]']
    for k, v in operators:
        for p in patterns:
            for t in all_types:
                opcodes += [(v, k, p % (t, t), 3)]

    # unary operators -- boolean
    operators = [('not', 'not')]
    patterns = ['%s->%s', '[%s]->[%s]']
    for k, v in operators:
        for p in patterns:
            for t in bool_types:
                opcodes += [(v, k, p % (t, t), 2)]

    # unary operators -- arithmetic
    operators = [('neg', '-'), ('pos', '+')]
    patterns = ['%s->%s', '[%s]->[%s]']
    for k, v in operators:
        for p in patterns:
            for t in arithmetic_types:
                opcodes += [(v, k, p % (t, t), 2)]

    # unary operators -- floating
    operators = [('sin', 'sin'), ('cos', 'cos'), ('tan', 'tan'),
                 ('asin', 'asin'), ('acos', 'acos'), ('atan', 'atan'),
                 ('sinh', 'sinh'), ('cosh', 'cosh'), ('tanh', 'tanh'),
                 ('asinh', 'asinh'), ('acosh', 'acosh'), ('atanh', 'atanh')]
    patterns = ['%s->%s', '[%s]->[%s]']
    for k, v in operators:
        for p in patterns:
            for t in float_types:
                opcodes += [(v, k, p % (t, t), 2)]

    # reduce aggregators
    operators = [('sum', 'sum'), ('prod', 'prod')]
    for k, v in operators:
        for t in arithmetic_types:
            opcodes += [(v, k, '[%s]->%s' % (t, t), 2)]

    # string concatenation
    operators = [('add', '+')]
    patterns = ['(%s,%s)->%s',     '(%s,[%s])->[%s]',
                '([%s],%s)->[%s]', '([%s],[%s])->[%s]']
    for k, v in operators:
        for p in patterns:
            for t in string_types:
                opcodes += [(v, k, p % (t, t, t), 3)]
    operators = [('sum', 'sum')]
    for k, v in operators:
        for t in string_types:
            opcodes += [(v, k, '[%s]->%s' % (t, t), 2)]

    # time arithmetic
    operators = [('sub', '-')]
    patterns = ['(%s,%s)->Timedelta',     '(%s,[%s])->[Timedelta]',
                '([%s],%s)->[Timedelta]', '([%s],[%s])->[Timedelta]']
    for k, v in operators:
        for p in patterns:
            for t in time_ish_types:
                opcodes += [(v, k, p % (t, t), 3)]
    operators = [('add', '+'), ('sub', '-'), ('mul', '*'), ('div', '/'),
                 ('bar', 'bar')]
    patterns = ['(%s,%s)->%s',     '(%s,[%s])->[%s]',
                '([%s],%s)->[%s]', '([%s],[%s])->[%s]']
    for k, v in operators:
        for p in patterns:
            for t1 in time_ish_types + timedelta_types:
                for t2 in timedelta_types:
                    opcodes += [(v, k, p % (t1, t2, t1), 3)]
    operators = [('add', '+'), ('mul', '*')]
    patterns = ['(%s,%s)->%s',     '(%s,[%s])->[%s]',
                '([%s],%s)->[%s]', '([%s],[%s])->[%s]']
    for k, v in operators:
        for p in patterns:
            for t1 in timedelta_types:
                for t2 in time_ish_types:
                    opcodes += [(v, k, p % (t1, t2, t2), 3)]
    operators = [('add', '+')]
    patterns = ['(%s,%s)->Timestamp',     '(%s,[%s])->[Timestamp]',
                '([%s],%s)->[Timestamp]', '([%s],[%s])->[Timestamp]']
    for k, v in operators:
        for p in patterns:
            for t1 in date_types:
                for t2 in time_types:
                    opcodes += [(v, k, p % (t1, t2), 3)]

    # timedelta literals
    units = ['ns', 'us', 'ms', 's', 'm', 'h', 'd']
    for u in units:
        opcodes += [('suffix' + u, 'unit_' + u, 'Int64->Timedelta', 2)]

    # wrappers
    operators = [('len', 'len'), ('count', 'count')]
    for k, v in operators:
        for t in all_types:
            opcodes += [(v, k, '[%s]->Int64' % t, 2)]
    operators = [('range', 'range')]
    for k, v in operators:
        for t in integer_types:
            opcodes += [(v, k, '%s->[%s]' % (t, t), 2)]

    # manually defined operators -- scalar and vector
    operators = [('del', 1)]
    patterns = ['%s', '[%s]']
    for k, v in operators:
        for p in patterns:
            for t in all_types:
                opcodes += [('', k, p % t, v)]

    # manually defined operators -- vector only
    operators = [('idx', 3)]
    for k, v in operators:
        for t in all_types:
            opcodes += [('', k, '([%s],Int64)->%s' % (t, t), v)]

    return opcodes

opcodes = _make_opcodes()


_vvm_types = {t[0]: t[1] for t in types}
_cpp_types = {t[0]: t[2] for t in types}


def get_opcode(func_name, type_sig):
    """ Generate op code for a function name and type signature """
    def get_vvm_type(t, append='s'):
        """ Convert Empirical's type to VVM's type  """
        if t[0] == '[' and t[-1] == ']':
            return get_vvm_type(t[1:-1], 'v')
        return _vvm_types[t] + append

    if len(type_sig) == 0:
        return func_name
    types = type_sig.split('->')
    params = types[0]
    suffix = ''
    if params[0] == '(' and params[-1] == ')':
        ps = params[1:-1].split(',')
        for p in ps:
            suffix += '_' + get_vvm_type(p)
    else:
        suffix = '_' + get_vvm_type(params)
    if func_name == 'cast':
        suffix += '_' + get_vvm_type(types[1])
    return func_name + suffix


def get_cpp_func(func_name, type_sig):
    """ Generate C++ function call for name and type signature """
    def get_cpp_type(t, append='s'):
        """ Convert Empirical's type to s/v suffix and C++'s type """
        if t[0] == '[' and t[-1] == ']':
            return get_cpp_type(t[1:-1], 'v')
        typestr = _cpp_types[t] if t != '()' else 'int64_t'
        return (append, typestr)

    if len(type_sig) == 0:
        return func_name
    halves = type_sig.split('->')
    params = halves[0]
    if params[0] == '(' and params[-1] == ')':
        ps = params[1:-1].split(',')
        results = map(get_cpp_type, ps)
        (suffixes, types) = zip(*results)
        suffix = ''.join(suffixes)
        template = ', '.join(types)
    else:
        (suffix, template) = get_cpp_type(params)
    if len(halves) == 2:
        template += ', ' + get_cpp_type(halves[1])[1]
    return func_name + '_' + suffix + '<' + template + '>'


def get_func_type(type_sig):
    """ Generate compiler-friendly syntax for Empirical's type """
    def gen_vvm_type(t):
        if t[0] == '[' and t[-1] == ']':
            return 'HIR::Array(%s)' % gen_vvm_type(t[1:-1])
        return "HIR::VVMType(size_t(VVM::vvm_types::%ss))" % _vvm_types[t]

    sigs = type_sig.split('->')
    argtypes = []
    if (len(sigs) == 2):
        (params, rettype) = sigs
        if params[0] == '(' and params[-1] == ')':
            ps = params[1:-1].split(',')
        else:
            ps = [params]
        for p in ps:
            argtypes += [gen_vvm_type(p)]
    else:
        rettype = sigs[0]
    rt = gen_vvm_type(rettype) if rettype != '()' else 'HIR::Void()'
    return '{%s}, %s' % (', '.join(argtypes), rt)


TABSIZE = 2
MAX_COL = 80


def reflow_lines(s, depth):
    """Reflow the line s indented depth tabs.

    Return a sequence of lines where no line extends beyond MAX_COL
    when properly indented.  The first line is properly indented based
    exclusively on depth * TABSIZE.  All following lines -- these are
    the reflowed lines generated by this function -- start at the same
    column as the first character beyond the opening { in the first
    line.
    """
    size = MAX_COL - depth * TABSIZE
    if len(s) < size:
        return [s]

    lines = []
    cur = s
    padding = ""
    while len(cur) > size:
        i = cur.rfind(' ', 0, size)
        if (i == -1):
            i = cur.find(' ')
        # XXX make sure we aren't in a quotation
        if cur[:i].count('"') % 2 == 1:
            i = cur.rfind('"', 0, i) - 1
        lines.append(padding + cur[:i])
        if len(lines) == 1:
            # find new size based on brace
            j = cur.find('{', 0, i)
            if j >= 0:
                j += 2  # account for the brace and the space after it
                size -= j
                padding = " " * j
            else:
                j = cur.find('(', 0, i)
                if j >= 0:
                    j += 1  # account for the paren (no space after it)
                    size -= j
                    padding = " " * j
        cur = cur[i+1:]
    else:
        lines.append(padding + cur)
    return lines


class HeaderWriter:
    """ Base class for writing headers; override run() in derived class """

    def __init__(self, filename):
        self.filename = filename

    def execute(self, auto_gen_msg, output_directory):
        """ Write the file contents """
        output_file = os.sep.join([output_directory, self.filename])
        self.file = open(output_file, "w")
        self.file.write(auto_gen_msg)
        self.file.write("#pragma once\n\n")
        self.run()
        self.file.close()

    def emit(self, s, depth=0):
        """ Emit a line, reflowing as needed """
        lines = reflow_lines(s, depth)
        for line in lines:
            line = (" " * TABSIZE * depth) + line + "\n"
            self.file.write(line)

    def run(self):
        pass


class TypesWriter(HeaderWriter):
    """ Write type logic """

    def run(self):
        self.emit('#include <string>')
        self.emit('')
        self.emit('namespace VVM {')
        all_types = [p for t in types for p in (t[1] + 's', t[1] + 'v')]
        type_labels = ", ".join(all_types)
        self.emit('enum class vvm_types: uint64_t { %s };' % type_labels)
        self.emit('')
        type_strings = ", ".join(['"' + t + '"' for t in all_types])
        self.emit('static std::string type_strings[] = { %s };' % type_strings)
        self.emit('')
        emp_types = [t[0] for t in types]
        emp_strings = ", ".join(['"%s", "[%s]"' % (t, t) for t in emp_types])
        self.emit('static std::string empirical_type_strings[] = { %s };' %
                  emp_strings)
        self.emit('}  // namespace VVM')
        self.emit('')


class OpcodesWriter(HeaderWriter):
    """ Write opcode strings """

    def run(self):
        self.emit('#include <string>')
        self.emit('')
        self.emit('namespace VVM {')
        opcode_labels = ", ".join([get_opcode(o[1], o[2])
                                   for o in opcodes])
        self.emit('enum class opcodes: uint64_t { %s };' % opcode_labels)
        self.emit('')
        opcode_labels = ", ".join(['"' + get_opcode(o[1], o[2]) + '"'
                                   for o in opcodes])
        self.emit('static std::string opcode_strings[] = { %s };' %
                  opcode_labels)
        self.emit('}  // namespace VVM')
        self.emit('')


class AllocateWriter(HeaderWriter):
    """ Write allocate logic """

    def run(self):
        self.emit('void* allocate_builtin(vvm_types t) {')
        self.emit('switch (t) {', 1)
        for t in types:
            self.emit('case vvm_types::%ss:' % t[1], 2)
            self.emit('return reinterpret_cast<void*>(new %s);' % t[2], 3)
            self.emit('case vvm_types::%sv:' % t[1], 2)
            self.emit('return reinterpret_cast<void*>(new std::vector<%s>);' %
                      t[2], 3)
        self.emit('}', 1)
        self.emit('}')
        self.emit('')


class BuiltinsWriter(HeaderWriter):
    """ Write builtin functions for Emprical """

    def run(self):
        self.emit('/* type definitions */')
        for t in types:
            ref = "HIR::VVMTypeRef(size_t(VVM::vvm_types::%ss))" % t[1]
            store = 'store_symbol("%s", %s);' % (t[0], ref)
            self.emit(store)
        self.emit('')
        self.emit('')

        # function traits
        none = 0; pure = 1; transform = 2; linear = 4; autostream = 8
        all_traits = pure | transform | linear
        io_traits = transform | linear  # input/output
        ca_traits = pure | linear       # change array
        ra_traits = pure | transform    # random access

        for o in opcodes:
            if len(o[0]) != 0:
                traits = all_traits
                d = {'load': autostream,
                     'store': none,
                     'print': none,
                     'unique': ca_traits,
                     'idx': ra_traits,
                     'multidx': ra_traits,
                     'sort': ra_traits,
                     'range': all_traits | autostream,
                     'now': io_traits}
                if o[0] in d:
                  traits = d[o[0]]
                oc = get_opcode(o[1], o[2])
                comment = '// "%s" %s %s' % (o[0], oc, o[2])
                self.emit(comment)
                ft = get_func_type(o[2])
                oc_enum = "size_t(VVM::opcodes::%s)" % oc
                ref = '%s, HIR::FuncType(%s, %d)' % (oc_enum, ft, traits)
                store = ('store_symbol("%s", HIR::VVMOpRef(%s));' %
                         (o[0], ref))
                self.emit(store)
                self.emit('')


class DisassemblerWriter(HeaderWriter):
    """ Write disassembler logic """

    def run(self):
        self.emit('#include <vector>')
        self.emit('#include <string>')
        self.emit('#include <sstream>')
        self.emit('')
        self.emit('#include <VVM/opcodes.h>')
        self.emit('')
        self.emit('namespace VVM {')
        self.emit('std::string dis_code(const instructions_t& code,'
                  ' size_t ip, size_t n) {')
        self.emit('std::string result;', 1)
        self.emit('for (size_t end = ip + n; ip < end; ip++) {', 1)
        self.emit('result += " " + decode_operand(code[ip]);', 2)
        self.emit('}', 1)
        self.emit('return result;', 1)
        self.emit('}')
        self.emit('')
        self.emit('#ifndef _MSC_VER')
        self.emit('std::string disassemble(const instructions_t& code) {')
        opcode_labels = ", ".join(["&&" + get_opcode(o[1], o[2])
                                   for o in opcodes])
        self.emit('static void* opcode_labels[] = { %s };' % opcode_labels, 1)
        self.emit('')
        self.emit('std::ostringstream oss;', 1)
        self.emit('size_t p, ip = 0;', 1)
        self.emit('goto *opcode_labels[code[ip]];', 1)
        self.emit('')
        for o in opcodes:
            oc = get_opcode(o[1], o[2])
            self.emit("%s:" % oc, 1)
            if oc == 'halt':
                self.emit('return oss.str();', 2)
            else:
                self.emit('p = ip;', 2)
                self.emit('ip += %d;' % (o[3] + 1), 2)
                s = ['oss', '"%s"' % oc]
                s += ['" " << decode_operand(code[p + %d])' % (i+1)
                      for i in range(o[3])]
                if oc == 'call':
                    s += ['dis_code(code, ip, (code[p + %d] >> 2))' % o[3]]
                s += ['std::endl;']
                self.emit(' << '.join(s), 2)
                if oc == 'call':
                    self.emit('ip += (code[p + %d] >> 2);' % o[3], 2)
                self.emit("goto *opcode_labels[code[ip]];", 2)
        self.emit('}')
        self.emit('#else  // _MSC_VER')
        self.emit('std::string disassemble(const instructions_t& code) {')
        self.emit('std::ostringstream oss;', 1)
        self.emit('size_t p, ip = 0;', 1)
        self.emit('while (true) {', 1)
        self.emit('switch (opcodes(code[ip])) {', 2)
        for o in opcodes:
            oc = get_opcode(o[1], o[2])
            self.emit("case opcodes::%s:" % oc, 3)
            if oc == 'halt':
                self.emit('return oss.str();', 4)
            else:
                self.emit('p = ip;', 4)
                self.emit('ip += %d;' % (o[3] + 1), 4)
                s = ['oss', '"%s"' % oc]
                s += ['" " << decode_operand(code[p + %d])' % (i+1)
                      for i in range(o[3])]
                if oc == 'call':
                    s += ['dis_code(code, ip, (code[p + %d] >> 2))' % o[3]]
                s += ['std::endl;']
                self.emit(' << '.join(s), 4)
                if oc == 'call':
                    self.emit('ip += (code[p + %d] >> 2);' % o[3], 4)
                self.emit("break;", 4)
        self.emit('}', 2)
        self.emit('}', 1)
        self.emit('}')
        self.emit('#endif  // _MSC_VER')
        self.emit('}  // namespace VVM')


class DispatchWriter(HeaderWriter):
    """ Write interpreter logic """

    def run(self):
        self.emit('#ifndef _MSC_VER')
        self.emit('void dispatch(const instructions_t& code) {')
        opcode_labels = ", ".join(["&&" + get_opcode(o[1], o[2])
                                   for o in opcodes])
        self.emit('static void* opcode_labels[] = { %s };' % opcode_labels, 1)
        self.emit('')
        self.emit('size_t p;', 1)
        self.emit('ip_ = 0;', 1)
        self.emit('goto *opcode_labels[code[ip_]];', 1)
        self.emit('')
        for o in opcodes:
            oc = get_opcode(o[1], o[2])
            self.emit("%s:" % oc, 1)
            if oc == 'halt':
                self.emit('return;', 2)
            else:
                self.emit('p = ip_;', 2)
                self.emit('ip_ += %d;' % (o[3] + 1), 2)
                args = ['code[p + %d]' % (i+1) for i in range(o[3])]
                if oc in ['ret', 'call']:
                    args += ['code']
                s = ', '.join(args)
                name = get_cpp_func(o[1], o[2])
                self.emit("%s(%s);" % (name, s), 2)
                self.emit("goto *opcode_labels[code[ip_]];", 2)
        self.emit('}')
        self.emit('#else  // _MSC_VER')
        self.emit('void dispatch(const instructions_t& code) {')
        self.emit('size_t p;', 1)
        self.emit('ip_ = 0;', 1)
        self.emit('while (true) {', 1)
        self.emit('switch (opcodes(code[ip_])) {', 2)
        for o in opcodes:
            oc = get_opcode(o[1], o[2])
            self.emit("case opcodes::%s:" % oc, 3)
            if oc == 'halt':
                self.emit('return;', 4)
            else:
                self.emit('p = ip_;', 4)
                self.emit('ip_ += %d;' % (o[3] + 1), 4)
                args = ['code[p + %d]' % (i+1) for i in range(o[3])]
                if oc in ['ret', 'call']:
                    args += ['code']
                s = ', '.join(args)
                name = get_cpp_func(o[1], o[2])
                self.emit("%s(%s);" % (name, s), 4)
                self.emit("break;", 4)
        self.emit('}', 2)
        self.emit('}', 1)
        self.emit('}')
        self.emit('#endif  // _MSC_VER')


class ReprWriter(HeaderWriter):
    """ Write repr logic """

    def run(self):
        self.emit('std::string represent_builtin(vvm_types t, operand_t o) {')
        self.emit('switch (t) {', 1)
        for t in types:
            self.emit('case vvm_types::%ss:' % t[1], 2)
            self.emit('return represent_s<%s>(o);' % t[2], 3)
            self.emit('case vvm_types::%sv:' % t[1], 2)
            self.emit('return represent_v<%s>(o);' % t[2], 3)
        self.emit('}', 1)
        self.emit('}')
        self.emit('')
        self.emit('std::vector<std::string> stringify(vvm_types t,'
                  ' Value v, std::string& s, size_t n) {')
        self.emit('switch (t) {', 1)
        for t in types:
            self.emit('case vvm_types::%ss:' % t[1], 2)
            self.emit('return stringify_wrap<%s>(v, s, n);' % t[2], 3)
            self.emit('case vvm_types::%sv:' % t[1], 2)
            self.emit('return stringify_v<%s>(v, s, n);' % t[2], 3)
        self.emit('}', 1)
        self.emit('}')
        self.emit('')


class ParseWriter(HeaderWriter):
    """ Write parser logic """

    def run(self):
        self.emit('void parse_array(vvm_types t,'
                  ' const std::vector<std::string>& s, Value v) {')
        self.emit('switch (t) {', 1)
        for t in types:
            self.emit('case vvm_types::%ss:' % t[1], 2)
            self.emit('return parse_array<%s>(s, v);' % t[2], 3)
            self.emit('case vvm_types::%sv:' % t[1], 2)
            self.emit('return parse_array<%s>(s, v);' % t[2], 3)
        self.emit('}', 1)
        self.emit('}')
        self.emit('')


class AssignWriter(HeaderWriter):
    """ Write assign logic """

    def run(self):
        self.emit('void assign_builtin(vvm_types t,'
                  ' operand_t v1, operand_t v2) {')
        self.emit('switch (t) {', 1)
        for t in types:
            self.emit('case vvm_types::%ss:' % t[1], 2)
            self.emit('return assign_builtin_s<%s>(v1, v2);' % t[2], 3)
            self.emit('case vvm_types::%sv:' % t[1], 2)
            self.emit('return assign_builtin_v<%s>(v1, v2);' % t[2], 3)
        self.emit('}', 1)
        self.emit('}')
        self.emit('')
        self.emit('void assign_value(vvm_types t, Value v1, Value v2) {')
        self.emit('switch (t) {', 1)
        for t in types:
            self.emit('case vvm_types::%ss:' % t[1], 2)
            self.emit('return assign_value_s<%s>(v1, v2);' % t[2], 3)
            self.emit('case vvm_types::%sv:' % t[1], 2)
            self.emit('return assign_value_v<%s>(v1, v2);' % t[2], 3)
        self.emit('}', 1)
        self.emit('}')
        self.emit('')


class AppendWriter(HeaderWriter):
    """ Write append logic """

    def run(self):
        self.emit('void append_builtin(vvm_types t,'
                  ' operand_t v1, operand_t v2) {')
        self.emit('switch (t) {', 1)
        for t in types:
            self.emit('case vvm_types::%ss:' % t[1], 2)
            self.emit('return append_s<%s>(v1, v2);' % t[2], 3)
            self.emit('case vvm_types::%sv:' % t[1], 2)
            self.emit('return append_s<%s>(v1, v2);' % t[2], 3)
        self.emit('}', 1)
        self.emit('}')
        self.emit('')


class WhereWriter(HeaderWriter):
    """ Write where logic """

    def run(self):
        self.emit('template<class T>')
        self.emit('void where_elem(vvm_types t, Value s,'
                  ' const std::vector<T>& tr, Value d) {')
        self.emit('switch (t) {', 1)
        for t in types:
            self.emit('case vvm_types::%ss:' % t[1], 2)
            self.emit('return where_elem<%s>(s, tr, d);' % t[2], 3)
            self.emit('case vvm_types::%sv:' % t[1], 2)
            self.emit('return where_elem<%s>(s, tr, d);' % t[2], 3)
        self.emit('}', 1)
        self.emit('}')
        self.emit('')


class WrapImmediateWriter(HeaderWriter):
    """ Write wrap_immediate logic """

    def run(self):
        self.emit('void* wrap_immediate(vvm_types t, operand_t o) {')
        self.emit('switch (t) {', 1)
        for t in types:
            self.emit('case vvm_types::%ss:' % t[1], 2)
            self.emit('return reinterpret_cast<void*>(', 3)
            self.emit('  new %s(get_value<%s>(o)));' % (t[2], t[2]), 3)
            self.emit('case vvm_types::%sv:' % t[1], 2)
            self.emit('return reinterpret_cast<void*>(new std::vector<%s>);' %
                      t[2], 3)
        self.emit('}', 1)
        self.emit('}')
        self.emit('')


class LenWriter(HeaderWriter):
    """ Write len logic """

    def run(self):
        self.emit('int64_t len(vvm_types t, Value s) {')
        self.emit('switch (t) {', 1)
        for t in types:
            self.emit('case vvm_types::%ss:' % t[1], 2)
            self.emit('return 1;', 3)
            self.emit('case vvm_types::%sv:' % t[1], 2)
            self.emit('return len<%s>(s);' % t[2], 3)
        self.emit('}', 1)
        self.emit('}')
        self.emit('')


class IsortWriter(HeaderWriter):
    """ Write isort logic """

    def run(self):
        self.emit('void isort_elem(vvm_types t, Value s,'
                  ' std::vector<int64_t>& i) {')
        self.emit('switch (t) {', 1)
        for t in types:
            self.emit('case vvm_types::%ss:' % t[1], 2)
            self.emit('return isort_elem<%s>(s, i);' % t[2], 3)
            self.emit('case vvm_types::%sv:' % t[1], 2)
            self.emit('return isort_elem<%s>(s, i);' % t[2], 3)
        self.emit('}', 1)
        self.emit('}')
        self.emit('')


class SplitWriter(HeaderWriter):
    """ Write split logic """

    def run(self):
        self.emit('void split_col(vvm_types t, size_t c,'
                  ' const std::vector<std::vector<int64_t>>& ig,'
                  ' const Dataframe& df, std::vector<Dataframe*>& td) {')
        self.emit('switch (t) {', 1)
        for t in types:
            self.emit('case vvm_types::%ss:' % t[1], 2)
            self.emit('return split_col<%s>(c, ig, df, td);' % t[2], 3)
            self.emit('case vvm_types::%sv:' % t[1], 2)
            self.emit('return split_col<%s>(c, ig, df, td);' % t[2], 3)
        self.emit('}', 1)
        self.emit('}')
        self.emit('')


class CategorizeWriter(HeaderWriter):
    """ Write categorize logic """

    def run(self):
        self.emit('int64_t categorize(vvm_types t, Value k,'
                  ' std::vector<int64_t>& l,'
                  ' size_t s) {')
        self.emit('switch (t) {', 1)
        for t in types:
            self.emit('case vvm_types::%ss:' % t[1], 2)
            self.emit('return categorize<%s>(k, l, s);' % t[2], 3)
            self.emit('case vvm_types::%sv:' % t[1], 2)
            self.emit('return categorize<%s>(k, l, s);' % t[2], 3)
        self.emit('}', 1)
        self.emit('}')
        self.emit('')
        self.emit('int64_t categorize2(vvm_types t, Value lk, Value rk,'
                  ' std::vector<int64_t>& ll, std::vector<int64_t>& rl,'
                  ' size_t s) {')
        self.emit('switch (t) {', 1)
        for t in types:
            self.emit('case vvm_types::%ss:' % t[1], 2)
            self.emit('return categorize2<%s>(lk, rk, ll, rl, s);' % t[2], 3)
            self.emit('case vvm_types::%sv:' % t[1], 2)
            self.emit('return categorize2<%s>(lk, rk, ll, rl, s);' % t[2], 3)
        self.emit('}', 1)
        self.emit('}')
        self.emit('')


class AsofWriter(HeaderWriter):
    """ Write asof logic """

    def run(self):
        self.emit('void asofmatch_arr(vvm_types t, operand_t l, operand_t r,'
                  ' bool s, AsofDirection d, std::vector<int64_t>& li,'
                  ' std::vector<int64_t>& ri) {')
        self.emit('switch (t) {', 1)
        for t in types:
            self.emit('case vvm_types::%ss:' % t[1], 2)
            self.emit('return asofmatch_arr<%s>(l, r, s, d, li, ri);' %
                      t[2], 3)
            self.emit('case vvm_types::%sv:' % t[1], 2)
            self.emit('return asofmatch_arr<%s>(l, r, s, d, li, ri);' %
                      t[2], 3)
        self.emit('}', 1)
        self.emit('}')
        self.emit('')
        self.emit('void asofnear_arr(vvm_types t, operand_t l, operand_t r,'
                  ' bool s, AsofDirection d, std::vector<int64_t>& li,'
                  ' std::vector<int64_t>& ri) {')
        self.emit('std::logic_error err("Invalid asofnear type");', 1)
        self.emit('switch (t) {', 1)
        for t in types:
            if (t[0] in arithmetic_types + time_ish_types):
                self.emit('case vvm_types::%ss:' % t[1], 2)
                self.emit('return asofnear_arr<%s>(l, r, s, d, li, ri);' %
                          t[2], 3)
                self.emit('case vvm_types::%sv:' % t[1], 2)
                self.emit('return asofnear_arr<%s>(l, r, s, d, li, ri);' %
                          t[2], 3)
            else:
                # Empirical's type check should prevent err
                self.emit('case vvm_types::%ss:' % t[1], 2)
                self.emit('throw err;', 3)
                self.emit('case vvm_types::%sv:' % t[1], 2)
                self.emit('throw err;', 3)
        self.emit('}', 1)
        self.emit('}')
        self.emit('')
        self.emit('void asofwithin_arr(vvm_types t, operand_t l, operand_t r,'
                  ' bool s, AsofDirection d, operand_t w, '
                  ' std::vector<int64_t>& li, std::vector<int64_t>& ri) {')
        self.emit('std::logic_error err("Invalid asofwithin type");', 1)
        self.emit('switch (t) {', 1)
        for t in types:
            if (t[0] in arithmetic_types + time_ish_types):
                self.emit('case vvm_types::%ss:' % t[1], 2)
                self.emit('return asofwithin_arr<%s>(l, r, s, d, w, li, ri);' %
                          t[2], 3)
                self.emit('case vvm_types::%sv:' % t[1], 2)
                self.emit('return asofwithin_arr<%s>(l, r, s, d, w, li, ri);' %
                          t[2], 3)
            else:
                # Empirical's type check should prevent err
                self.emit('case vvm_types::%ss:' % t[1], 2)
                self.emit('throw err;', 3)
                self.emit('case vvm_types::%sv:' % t[1], 2)
                self.emit('throw err;', 3)
        self.emit('}', 1)
        self.emit('}')
        self.emit('')
        self.emit('void eqasofmatch_df(vvm_types t1, operand_t t2,'
                  ' operand_t l1, operand_t r1, operand_t l2, operand_t r2,'
                  ' bool s, AsofDirection d, std::vector<int64_t>& li,'
                  ' std::vector<int64_t>& ri) {')
        self.emit('switch (t1) {', 1)
        for t in types:
            self.emit('case vvm_types::%ss:' % t[1], 2)
            self.emit(
              'return eqasofmatch_df<%s>(t2, l1, r1, l2, r2, s, d, li, ri);' %
              t[2], 3)
            self.emit('case vvm_types::%sv:' % t[1], 2)
            self.emit(
              'return eqasofmatch_df<%s>(t2, l1, r1, l2, r2, s, d, li, ri);' %
              t[2], 3)
        self.emit('}', 1)
        self.emit('}')
        self.emit('')
        self.emit('void eqasofnear_df(vvm_types t1, operand_t t2,'
                  ' operand_t l1, operand_t r1, operand_t l2, operand_t r2,'
                  ' bool s, AsofDirection d, std::vector<int64_t>& li,'
                  ' std::vector<int64_t>& ri) {')
        self.emit('std::logic_error err("Invalid eqasofnear type");', 1)
        self.emit('switch (t1) {', 1)
        for t in types:
            if (t[0] in arithmetic_types + time_ish_types):
              self.emit('case vvm_types::%ss:' % t[1], 2)
              self.emit(
                'return eqasofnear_df<%s>(t2, l1, r1, l2, r2, s, d, li, ri);' %
                t[2], 3)
              self.emit('case vvm_types::%sv:' % t[1], 2)
              self.emit(
                'return eqasofnear_df<%s>(t2, l1, r1, l2, r2, s, d, li, ri);' %
                t[2], 3)
            else:
                # Empirical's type check should prevent err
                self.emit('case vvm_types::%ss:' % t[1], 2)
                self.emit('throw err;', 3)
                self.emit('case vvm_types::%sv:' % t[1], 2)
                self.emit('throw err;', 3)
        self.emit('}', 1)
        self.emit('}')
        self.emit('')
        self.emit('void eqasofwithin_df(vvm_types t1, operand_t t2,'
                  ' operand_t l1, operand_t r1, operand_t l2, operand_t r2,'
                  ' bool s, AsofDirection d, operand_t w,'
                  ' std::vector<int64_t>& li, std::vector<int64_t>& ri) {')
        self.emit('std::logic_error err("Invalid eqasofwithin type");', 1)
        self.emit('switch (t1) {', 1)
        for t in types:
            if (t[0] in arithmetic_types + time_ish_types):
              self.emit('case vvm_types::%ss:' % t[1], 2)
              self.emit(
                'return eqasofwithin_df<%s>(t2, l1, r1, l2, r2, s, d, w, li, ri);' %
                t[2], 3)
              self.emit('case vvm_types::%sv:' % t[1], 2)
              self.emit(
                'return eqasofwithin_df<%s>(t2, l1, r1, l2, r2, s, d, w, li, ri);' %
                t[2], 3)
            else:
                # Empirical's type check should prevent err
                self.emit('case vvm_types::%ss:' % t[1], 2)
                self.emit('throw err;', 3)
                self.emit('case vvm_types::%sv:' % t[1], 2)
                self.emit('throw err;', 3)
        self.emit('}', 1)
        self.emit('}')
        self.emit('')


class ChainOfWriters:
    def __init__(self, auto_gen_msg, output_directory):
        self.auto_gen_msg = auto_gen_msg
        self.output_directory = output_directory

    def run(self, *writers):
        for w in writers:
            w.execute(self.auto_gen_msg, self.output_directory)


def main(output_directory):
    # determine "auto gen" message to appear at top of output
    argv0 = sys.argv[0]
    components = argv0.split(os.sep)
    argv0 = os.sep.join(components[-2:])
    common_msg = "/* File automatically generated by %s. */\n\n"
    auto_gen_msg = common_msg % argv0

    # run through all headers
    if not os.path.exists(output_directory):
        os.makedirs(output_directory)
    c = ChainOfWriters(auto_gen_msg, output_directory)
    c.run(TypesWriter('types.h'),
          OpcodesWriter('opcodes.h'),
          AllocateWriter('allocate.h'),
          BuiltinsWriter('builtins.h'),
          DisassemblerWriter('disassembler.h'),
          DispatchWriter('dispatch.h'),
          ReprWriter('repr.h'),
          ParseWriter('parse.h'),
          AssignWriter('assign.h'),
          AppendWriter('append.h'),
          WhereWriter('where.h'),
          WrapImmediateWriter('wrap_immediate.h'),
          LenWriter('len.h'),
          IsortWriter('isort.h'),
          SplitWriter('split.h'),
          CategorizeWriter('categorize.h'),
          AsofWriter('asof.h')
          )


if __name__ == "__main__":
    import getopt

    # get output directory
    output_directory = None
    opts, args = getopt.getopt(sys.argv[1:], "d:")
    for o, v in opts:
        if o == '-d':
            output_directory = v

    # ensure the parameters are present
    if not output_directory:
        print('Must specify output directory')
        sys.exit(1)
    if len(args) != 0:
        print('Unrecognized command-line options')
        sys.exit(1)

    # run everything
    main(output_directory)
