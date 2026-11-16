#!/usr/bin/env python

# DOF disassembler / Dumper
# See bsd/sys/dtrace.h for details on the DOF format


from __future__ import division, print_function

from builtins import hex
from builtins import range
from builtins import object

import lldb
import shlex

# DTrace Object Format (DOF) (from bsd/sys/dtrace.h)

DOF_ID_SIZE = 16

DOF_ID_MODEL = 4
DOF_ID_ENCODING = 5
DOF_ID_VERSION = 6
DOF_ID_DIFVERS = 7
DOF_ID_DIFIREG = 8
DOF_ID_DIFTREG = 9

dof_magic = [0x7f, ord('D'), ord('O'), ord('F')]

DOF_SECF_LOAD = 1

section_names = ["NONE", "COMMENTS", "SOURCE", "ECBDESC", "PROBEDESC",
        "ACTDESC", "DIFOHDR", "DIF", "STRTAB", "VARTAB",  "RELTAB", "TYPTAB",
        "URELHDR", "KRELHDR", "OPTDESC", "PROVIDER", "PROBES", "PRARGS",
        "PROFFS", "INTTAB", "UTSNAME", "XLTAB", "XLMEMBERS", "XLIMPORT",
        "XLEXPORT", "PREXPORT", "PRENOFFS"]


# DTrace DIF instructions (from bsd/sys/dtrace.h)
def DIF_INSTR_OP(i):
    return (((i) >> 24) & 0xff)
def DIF_INSTR_R1(i):
    return (((i) >> 16) & 0xff)
def DIF_INSTR_R2(i):
    return (((i) >>  8) & 0xff)
def DIF_INSTR_RD(i):
    return ((i) & 0xff)
def DIF_INSTR_RS(i):
    return ((i) & 0xff)
def DIF_INSTR_LABEL(i):
    return ((i) & 0xffffff)
def DIF_INSTR_VAR(i):
    return (((i) >>  8) & 0xffff)
def DIF_INSTR_INTEGER(i):
    return (((i) >>  8) & 0xffff)
def DIF_INSTR_STRING(i):
    return (((i) >>  8) & 0xffff)
def DIF_INSTR_SUBR(i):
    return (((i) >>  8) & 0xffff)
def DIF_INSTR_TYPE(i):
    return (((i) >> 16) & 0xff)
def DIF_INSTR_XLREF(i):
    return (((i) >>  8) & 0xffff)

# DTrace DIF variables
DIFV_SCOPE_GLOBAL = 0
DIFV_SCOPE_THREAD = 1
DIFV_SCOPE_LOCAL = 2

# DTrace subroutines (from bsd/sys/dtrace.h)
subroutine_strings = {
    0: "rand",
    1: "mutex_owned",
    2: "mutex_owner",
    3: "mutex_type_adaptive",
    4: "mutex_type_spin",
    5: "rw_read_held",
    6: "rw_write_held",
    7: "rw_iswriter",
    8: "copyin",
    9: "copyinstr",
    10: "speculation",
    11: "progenyof",
    12: "strlen",
    13: "copyout",
    14: "copyoutstr",
    15: "alloca",
    16: "bcopy",
    17: "copyinto",
    18: "msgdsize",
    19: "msgsize",
    20: "getmajor",
    21: "getminor",
    22: "ddi_pathname",
    23: "strjoin",
    24: "lltostr",
    25: "basename",
    26: "dirname",
    27: "cleanpath",
    28: "strchr",
    29: "strrchr",
    30: "strstr",
    31: "strtok",
    32: "substr",
    33: "index",
    34: "rindex",
    35: "htons",
    36: "htonl",
    37: "htonll",
    38: "ntohs",
    39: "ntohl",
    40: "ntohll",
    41: "inet_ntop",
    42: "inet_nota",
    43: "inet_nota6",
    44: "toupper",
    45: "tolower",

    # Apple specific subroutines
    200: "vm_kernel_addrperm",
    201: "kdebug_trace",
    202: "kdebug_trace_string"
}

# DTrace actions (from bsd/sys/dtrace.h)

DTRACEACT_NONE = 0
DTRACEACT_DIFEXPR = 1
DTRACEACT_EXIT = 2
DTRACEACT_PRINTF = 3
DTRACEACT_PRINTA = 4
DTRACEACT_LIBACT = 5
DTRACEACT_TRACEMEM = 6
DTRACEACT_TRACEMEM_DYNSIZE = 7

DTRACEACT_PROC = 0x0100
DTRACEACT_USTACK = DTRACEACT_PROC + 1
DTRACEACT_JSTACK = DTRACEACT_PROC + 2
DTRACEACT_USYM = DTRACEACT_PROC + 3
DTRACEACT_UMOD = DTRACEACT_PROC + 3
DTRACEACT_UADDR = DTRACEACT_PROC + 3

DTRACEACT_PROC_DESTRUCTIVE = 0x0200
DTRACEACT_STOP = DTRACEACT_PROC_DESTRUCTIVE + 1
DTRACEACT_RAISE = DTRACEACT_PROC_DESTRUCTIVE + 2
DTRACEACT_SYSTEM = DTRACEACT_PROC_DESTRUCTIVE + 3
DTRACEACT_FREOPEN = DTRACEACT_PROC_DESTRUCTIVE + 4

DTRACEACT_KERNEL = 0x0400
DTRACEACT_STACK = DTRACEACT_KERNEL + 1
DTRACEACT_SYM = DTRACEACT_KERNEL + 2
DTRACEACT_MOD = DTRACEACT_KERNEL + 3

DTRACEACT_KERNEL_DESTRUCTIVE = 0x0500
DTRACEACT_BREAKPOINT = DTRACEACT_KERNEL_DESTRUCTIVE + 1
DTRACEACT_PANIC = DTRACEACT_KERNEL_DESTRUCTIVE + 2
DTRACEACT_CHILL = DTRACEACT_KERNEL_DESTRUCTIVE + 3

DTRACEACT_SPECULATIVE = 0x0600
DTRACEACT_SPECULATE = DTRACEACT_SPECULATIVE + 1
DTRACEACT_COMMIT = DTRACEACT_SPECULATIVE + 2
DTRACEACT_DISCARD = DTRACEACT_SPECULATIVE + 3

DTRACEACT_AGGREGATION = 0x0700
DTRACEAGG_COUNT = DTRACEACT_AGGREGATION + 1
DTRACEAGG_MIN = DTRACEACT_AGGREGATION + 2
DTRACEAGG_MAX = DTRACEACT_AGGREGATION + 3
DTRACEAGG_AVG = DTRACEACT_AGGREGATION + 4
DTRACEAGG_SUM = DTRACEACT_AGGREGATION + 5
DTRACEAGG_STDDEV = DTRACEACT_AGGREGATION + 6
DTRACEAGG_QUANTIZE = DTRACEACT_AGGREGATION + 7
DTRACEAGG_LQUANTIZE = DTRACEACT_AGGREGATION + 8
DTRACEAGG_LLQUANTIZE = DTRACEACT_AGGREGATION + 9


err = lldb.SBError()


action_strings = {
    DTRACEACT_NONE: 'NONE',
    DTRACEACT_DIFEXPR: 'DIFEXPR',
    DTRACEACT_EXIT: 'EXIT',
    DTRACEACT_PRINTF: 'PRINTF',
    DTRACEACT_PRINTA: 'PRINTA',
    DTRACEACT_LIBACT: 'LIBACT',
    DTRACEACT_TRACEMEM: 'TRACEMEM',
    DTRACEACT_TRACEMEM_DYNSIZE: 'TRACEMEM_DYNSIZE',
    DTRACEACT_USTACK: 'USTACK',
    DTRACEACT_JSTACK: 'JSTACK',
    DTRACEACT_USYM: 'USYM',
    DTRACEACT_UMOD: 'UMOD',
    DTRACEACT_UADDR: 'UADDR',
    DTRACEACT_STOP: 'STOP',
    DTRACEACT_RAISE: 'RAISE',
    DTRACEACT_SYSTEM: 'SYSTEM',
    DTRACEACT_FREOPEN: 'FREOPEN',
    DTRACEACT_STACK: 'STACK',
    DTRACEACT_SYM: 'SYM',
    DTRACEACT_MOD: 'MOD',
    DTRACEACT_BREAKPOINT: 'BREAKPOINT',
    DTRACEACT_PANIC: 'PANIC',
    DTRACEACT_CHILL: 'CHILL',
    DTRACEACT_COMMIT: 'COMMIT',
    DTRACEACT_DISCARD: 'DISCARD',
    DTRACEAGG_COUNT: 'COUNT',
    DTRACEAGG_MIN: 'MIN',
    DTRACEAGG_MAX: 'MAX',
    DTRACEAGG_AVG: 'AVG',
    DTRACEAGG_SUM: 'SUM',
    DTRACEAGG_STDDEV: 'STDDEV',
    DTRACEAGG_QUANTIZE: 'QUANTIZE',
    DTRACEAGG_LQUANTIZE: 'LQUANTIZE',
    DTRACEAGG_LLQUANTIZE: 'LLQUANTIZE'
}

def ActionString(kind):
    if kind in action_strings:
        return action_strings[kind]
    return '??? ({})'.format(kind)

def DOFModelString(model):
    if model == 0:
        return 'NONE'
    elif model == 1:
        return 'ILP32'
    elif model == 2:
        return 'LP64'

def DOFEncodingString(encoding):
    if encoding == 0:
        return 'NONE'
    elif encoding == 1:
        return 'LSB'
    elif encoding == 2:
        return 'MSB'

def DOFFlagsString(flag):
    if flag != DOF_SECF_LOAD:
        return '(Non-Loadable)'
    return ''

def DOFSecIdxString(val):
    if val == -1:
        return 'NONE'
    return val

def GetChildAtIndexAsUnsigned(source, i):
    return source.GetChildAtIndex(i).GetValueAsUnsigned()

def GetMemberAsSigned(source, field):
    return source.GetChildMemberWithName(field, True).GetValueAsSigned()

def GetMemberAsUnsigned(source, field):
    return source.GetChildMemberWithName(field, True).GetValueAsUnsigned()

class DIFODisassembler(object):
    """
        Disassembler for DIFO given a DIF section, a strtab section, a inttab
        section and a vartab section
    """
    def __init__(self, dif, strtab, inttab, vartab):
        self.dif = dif
        self.strtab = strtab
        self.inttab = inttab
        self.vartab = vartab

        self.ops = [
            ["(illegal opcode)", self.DisStr],
            ["or", self.DisLog],                # DIF_OP_OR
            ["xor", self.DisLog],               # DIF_OP_XOR
            ["and", self.DisLog],               # DIF_OP_AND
            ["sll", self.DisLog],               # DIF_OP_SLL
            ["srl", self.DisLog],               # DIF_OP_SRL
            ["sub", self.DisLog],               # DIF_OP_SUB
            ["add", self.DisLog],               # DIF_OP_ADD
            ["mul", self.DisLog],               # DIF_OP_MUL
            ["sdiv", self.DisLog],              # DIF_OP_SDIV
            ["udiv", self.DisLog],              # DIF_OP_UDIV
            ["srem", self.DisLog],              # DIF_OP_SREM
            ["urem", self.DisLog],              # DIF_OP_UREM
            ["not", self.DisR1Rd],              # DIF_OP_NOT
            ["mov", self.DisR1Rd],              # DIF_OP_MOV
            ["cmp", self.DisCmp],               # DIF_OP_CMP
            ["tst", self.DisTst],               # DIF_OP_TST
            ["ba", self.DisBranch],             # DIF_OP_BA
            ["be", self.DisBranch],             # DIF_OP_BE
            ["bne", self.DisBranch],            # DIF_OP_BNE
            ["bg", self.DisBranch],             # DIF_OP_BG
            ["bgu", self.DisBranch],            # DIF_OP_BGU
            ["bge", self.DisBranch],            # DIF_OP_BGE
            ["bgeu", self.DisBranch],           # DIF_OP_BGEU
            ["bl", self.DisBranch],             # DIF_OP_BL
            ["blu", self.DisBranch],            # DIF_OP_BLU
            ["ble", self.DisBranch],            # DIF_OP_BLE
            ["bleu", self.DisBranch],           # DIF_OP_BLEU
            ["ldsb", self.DisLoad],             # DIF_OP_LDSB
            ["ldsh", self.DisLoad],             # DIF_OP_LDSH
            ["ldsw", self.DisLoad],             # DIF_OP_LDSW
            ["ldub", self.DisLoad],             # DIF_OP_LDUB
            ["lduh", self.DisLoad],             # DIF_OP_LDUH
            ["lduw", self.DisLoad],             # DIF_OP_LDUW
            ["ldx", self.DisLoad],              # DIF_OP_LDX
            ["ret", self.DisRet],               # DIF_OP_RET
            ["nop", self.DisStr],               # DIF_OP_NOP
            ["setx", self.DisSetX],             # DIF_OP_SETX
            ["sets", self.DisSetS],             # DIF_OP_SETS
            ["scmp", self.DisCmp],              # DIF_OP_SCMP
            ["ldga", self.DisLda],              # DIF_OP_LDGA
            ["ldgs", self.DisLdv],              # DIF_OP_LDGS
            ["stgs", self.DisStv],              # DIF_OP_STGS
            ["ldta", self.DisLda],              # DIF_OP_LDTA
            ["ldts", self.DisLdv],              # DIF_OP_LDTS
            ["stts", self.DisStv],              # DIF_OP_STTS
            ["sra", self.DisLog],               # DIF_OP_SRA
            ["call", self.DisCall],             # DIF_OP_CALL
            ["pushtr", self.DisPushTs],         # DIF_OP_PUSHTR
            ["pushtv", self.DisPushTs],         # DIF_OP_PUSHTS
            ["popts", self.DisStr],             # DIF_OP_POPTS
            ["flushts", self.DisStr],           # DIF_OP_FLUSHTS
            ["ldgaa", self.DisLdv],             # DIF_OP_LDGAA
            ["ldtaa", self.DisLdv],             # DIF_OP_LDTAA
            ["stgaa", self.DisStv],             # DIF_OP_STDGAA
            ["sttaa", self.DisStv],             # DIF_OP_STTAA
            ["ldls", self.DisLdv],              # DIF_OP_LDLS
            ["stls", self.DisStv],              # DIF_OP_STLS
            ["allocs", self.DisR1Rd],           # DIF_OP_ALLOCS
            ["copys", self.DisLog],             # DIF_OP_COPYS
            ["stb", self.DisStore],             # DIF_OP_STB
            ["sth", self.DisStore],             # DIF_OP_STH
            ["stw", self.DisStore],             # DIF_OP_STW
            ["stx", self.DisStore],             # DIF_OP_STX
            ["uldsb", self.DisLoad],            # DIF_OP_ULDSB
            ["uldsh", self.DisLoad],            # DIF_OP_ULDSH
            ["uldsw", self.DisLoad],            # DIF_OP_ULDSW
            ["uldub", self.DisLoad],            # DIF_OP_ULDUB
            ["ulduh", self.DisLoad],            # DIF_OP_ULDUH
            ["ulduw", self.DisLoad],            # DIF_OP_ULDUW
            ["uldx", self.DisLoad],             # DIF_OP_ULDX
            ["rldsb", self.DisLoad],            # DIF_OP_RLDSB
            ["rldsh", self.DisLoad],            # DIF_OP_RLDSH
            ["rldsw", self.DisLoad],            # DIF_OP_RLDSW
            ["rldub", self.DisLoad],            # DIF_OP_RLDUB
            ["rlduh", self.DisLoad],            # DIF_OP_RLDUH
            ["rlduw", self.DisLoad],            # DIF_OP_RLDUW
            ["rldx", self.DisLoad],             # DIF_OP_RLDX
            ["xlate", self.DisXlate],           # DIF_OP_XLATE
            ["xlarg", self.DisXlate],           # DIF_OP_XLARG
        ]
    def Disassemble(self):
        dif = self.dif
        print('\t%-3s %-8s    %s' % ('OFF', 'OPCODE', 'INSTRUCTION'))
        for i in range(dif.dif_len):
            instr = dif.GetInstr(i)
            op = DIF_INSTR_OP(instr)
            print('\t%02lu: %08x    ' % (i, instr), end=' ')
            if op < len(self.ops):
                self.ops[op][1](self.ops[op][0], instr)
            else:
                print('')
    def DisStr(self, name, ins):
        print(name)

    def DisLog(self, name, ins):
        print("%-4s %%r%u, %%r%u, %%r%u" % (name, DIF_INSTR_R1(ins),
                DIF_INSTR_R2(ins), DIF_INSTR_RD(ins)))

    def DisR1Rd(self, name, ins):
        print("%-4s %%r%u, %%r%u" % (name, DIF_INSTR_R1(ins), DIF_INSTR_RD(ins)))

    def DisCmp(self, name, ins):
        print("%-4s %%r%u, %%r%u" % (name, DIF_INSTR_R1(ins), DIF_INSTR_R2(ins)))

    def DisTst(self, name, ins):
        print("%-4s %%r%u" % (name, DIF_INSTR_R1(ins)))

    def DisBranch(self, name, ins):
        print("%-4s %u" % (name, DIF_INSTR_LABEL(ins)))

    def DisLoad(self, name, ins):
        print("%-4s [%%r%u], %%r%u" % (name, DIF_INSTR_R1(ins), DIF_INSTR_RD(ins)))

    def DisRet(self, name, ins):
        print("%-4s %%r%u" % (name, DIF_INSTR_RD(ins)))

    def DisSetX(self, name, ins):
        intptr = DIF_INSTR_INTEGER(ins)
        print("%-4s DT_INTEGER[%u], %%r%u" % (name, intptr, DIF_INSTR_RD(ins)), end=' ')
        if intptr < self.inttab.int_len:
            print("\t\t! 0x%dx" % (self.inttab.GetInt(intptr)))
        else:
            print('')

    def DisSetS(self, name, ins):
        strptr = DIF_INSTR_STRING(ins)
        print("%-4s DT_STRING[%u], %%r%u" % (name, strptr, DIF_INSTR_RD(ins)), end=' ')

        if strptr < self.strtab.data_size:
            print("\t\t! \"%s\"" % self.strtab.GetString(strptr))
        else:
            print('')
    def DisLda(self, name, ins):
        var = DIF_INSTR_R1(ins)
        print('%-4s DT_VAR(%u), %%r%u, %%r%u' % (name, var, DIF_INSTR_R2(ins),
                DIF_INSTR_RD(ins)), end=' ')

        vname = self.VarName(var, self.Scope(name))
        if vname is not(None):
            print("\t\t! DT_VAR(%u) = \"%s\"" % (var, vname))
        else:
            print('')

    def DisLdv(self, name, ins):
        var = DIF_INSTR_VAR(ins)
        print('%-4s DT_VAR(%u), %%r%u' % (name, var, DIF_INSTR_RD(ins)), end=' ')

        vname = self.VarName(var, self.Scope(name))
        if vname is not(None):
            print("\t\t! DT_VAR(%u) = \"%s\"" % (var, vname))
        else:
            print('')

    def DisStv(self, name, ins):
        var = DIF_INSTR_VAR(ins)
        print('%-4s %%r%u, DT_VAR(%u)' % (name, DIF_INSTR_RS(ins), var), end=' ')

        vname = self.VarName(var, self.Scope(name))
        if vname is not(None):
            print("\t\t! DT_VAR(%u) = \"%s\"" % (var, vname))
        else:
            print('')

    def DisLog(self, name, ins):
        print("%-4s %%r%u, %%r%u, %%r%u" % (name, DIF_INSTR_R1(ins),
                DIF_INSTR_R2(ins), DIF_INSTR_RD(ins)))

    def DisCall(self, name, ins):
        subr = DIF_INSTR_SUBR(ins)
        subr_string = subroutine_strings[subr] if subr in subroutine_strings else ''
        print('%-4s DIF_SUBR(%u), %%r%u\t\t! %s' % (name, subr,
                DIF_INSTR_RD(ins), subr_string))

    def DisPushTs(self, name, ins):
        tnames = ["D type", "string"]
        itype = DIF_INSTR_TYPE(ins)
        print("%-4s DT_TYPE(%u), %%r%u, %%r%u" % (name, itype, DIF_INSTR_R2(ins),
                DIF_INSTR_RS(ins)), end=' ')

        if itype <= 1:
            print("\t! DT_TYPE(%u) = %s" % (itype, tnames[itype]))
        else:
            print('')

    def DisStore(self, name, ins):
        print('%-4s %%r%u, [%%r%u]' % (DIF_INSTR_R1(ins), DIF_INSTR_RD(ins)))

    def DisLoad(self, name, ins):
        print('%-4s [%%r%u], %%r%u' % (name, DIF_INSTR_R1(ins), DIF_INSTR_RD(ins)))

    def DisXlate(self, name, ins):
        xlr = DIF_INSTR_XLREF(ins);
        print("%-4s DT_XLREF[%u], %%r%u" % (name, xlr, DIF_INSTR_RD(ins)))

    def VarName(self, vid, scope):
        for i in range(self.vartab.var_len):
            var = self.vartab.GetVar(i)
            tested_vid = GetMemberAsSigned(var, "dtdv_id")
            tested_scope = GetMemberAsSigned(var, "dtdv_scope")
            if vid == tested_vid and scope == tested_scope:
                name = GetMemberAsUnsigned(var, "dtdv_name")
                return self.strtab.GetString(name)
        return None

    def Scope(self, name):
        scope = name[2]
        if scope == 'l':
            return DIFV_SCOPE_LOCAL
        elif scope == 't':
            return DIFV_SCOPE_THREAD
        elif scope == 'g':
            return DIFV_SCOPE_GLOBAL
        else:
            return -1


class AddrData(object):
    """DOF data coming from a specific address"""
    def __init__(self, target, addr):
        self.target = target
        self.addr = addr

    def GetAddr(self):
        return self.addr

    def GetValue(self, name, offset, vtype):
        address = lldb.SBAddress(self.addr + offset, self.target)
        return self.target.CreateValueFromAddress(name, address, vtype)


class SectionData(object):
    """
        DOF data coming from a section. We can't use addresses directly here
        because lldb won't let us inspect DOF sections in libraries not loaded
        in a specific process otherwise
    """
    def __init__(self, target, section):
        self.section = section
        self.target = target

    def GetAddr(self):
        return self.section.GetLoadAddress(self.target)

    def GetData(self, offset, size):
        return self.section.GetSectionData(offset, size)

    def GetValue(self, name, offset, vtype):
        size = vtype.GetByteSize()
        return self.target.CreateValueFromData(name, self.GetData(offset, size),
                vtype)


class DOFHeader(object):
    def __init__(self, dof, target, data):
        self.dof = dof
        # Find the required DOF types
        dof_hdr_type = target.FindFirstType("dof_hdr_t")
        if not(dof_hdr_type.IsValid()):
            print('could not find dof_hdr_t type')
            return
        header = data.GetValue("$dof_hdr", 0, dof_hdr_type)
        self.secoff = GetMemberAsUnsigned(header, "dofh_secoff")
        self.secsize = GetMemberAsUnsigned(header,"dofh_secsize")
        self.nsecs = GetMemberAsUnsigned(header, "dofh_secnum")

        dofh_ident = header.GetChildMemberWithName("dofh_ident", True)
        for i in range(3):
            mag = dofh_ident.GetChildAtIndex(i).GetValueAsUnsigned()
            if mag != dof_magic[i]:
                raise LookupError("Invalid magic number, is this a DOF ?")
        self.model = DOFModelString(GetChildAtIndexAsUnsigned(dofh_ident, DOF_ID_MODEL))
        self.encoding = DOFEncodingString(GetChildAtIndexAsUnsigned(dofh_ident, DOF_ID_ENCODING))
        self.format_version = GetChildAtIndexAsUnsigned(dofh_ident, DOF_ID_VERSION)
        self.difvers = GetChildAtIndexAsUnsigned(dofh_ident, DOF_ID_DIFVERS)
        self.difireg = GetChildAtIndexAsUnsigned(dofh_ident, DOF_ID_DIFIREG)
        self.diftreg = GetChildAtIndexAsUnsigned(dofh_ident, DOF_ID_DIFTREG)

    def Show(self):
        print('DOF data model: {}'.format(self.model))
        print('DOF encoding: {}'.format(self.encoding))
        print('DOF format version: {}'.format(self.format_version))
        print('DOF instruction set version: {}'.format(self.difvers))
        print('DOF integer register count: {}'.format(self.difireg))
        print('DOF tuple register count: {}'.format(self.diftreg))


class DOFSection(object):
    def __init__(self, dof, target, data, i):
        self.hdr_offset = dof.header.secoff + dof.header.secsize * i
        sec = data.GetValue("$sec", self.hdr_offset, dof.dof_sec_type)
        self.i = i
        self.dof = dof
        self.stype = section_names[GetMemberAsUnsigned(sec, "dofs_type")]
        self.flags = DOFFlagsString(GetMemberAsUnsigned(sec, "dofs_flags"))
        self.ent_size = GetMemberAsUnsigned(sec, "dofs_entsize")
        self.offset = GetMemberAsUnsigned(sec, "dofs_offset")
        self.data_size = GetMemberAsUnsigned(sec, "dofs_size")

    def Show(self):
        print('Section {} type {} data size {} ent size {} {} header {} data {}'.format(
                self.i,
                self.stype,
                self.data_size, self.ent_size, self.flags,
                hex(self.dof.addr + self.hdr_offset),
                hex(self.dof.addr + self.offset)))


class DOFDIFSection(DOFSection):
    def __init__(self, dof, target, data, i):
        super(DOFDIFSection, self).__init__(dof, target, data, i)

        dif_instr_type = target.FindFirstType("dif_instr_t")
        if not(dif_instr_type):
            raise TypeError('Could not find dif_instr_t. Do you have a kernel binary loaded ?)')
        self.dif_len = self.data_size // dif_instr_type.GetByteSize()
        # Ridiculous hack here : We don't have SBType::GetArrayType because
        # our lldb is too old, so we declare a variable of type
        # pointer to dif_instr_t[number_of_elements] and we grab its Pointee type
        # (Note that we can't directly declare a variable of type
        # dif_instr_t[number_of_elements]
        # since lldb tends to truncate the array size of conveniance variables
        # when debugging remote kernels
        target.EvaluateExpression("dif_instr_t (*$pi{})[{}]".format(self.dif_len, self.dif_len))
        dift_type = target.EvaluateExpression("$pi{}".format(self.dif_len)).GetType().GetPointeeType()
        self.dif = data.GetValue("$dif_buf", self.offset, dift_type)

    def GetInstr(self, i):
        return GetChildAtIndexAsUnsigned(self.dif, i)


class DOFStrTabSection(DOFSection):
    def __init__(self, dof, target, data, i):
        super(DOFStrTabSection, self).__init__(dof, target, data, i)
        # See note in DOFDIFSection.__init__
        target.EvaluateExpression("char (*$p{})[{}]".format(self.data_size,
            self.data_size))
        char_type = target.EvaluateExpression("$p{}".format(self.data_size)).GetType().GetPointeeType()
        self.str_sec_data = data.GetValue("$strtab", self.offset, char_type)

    def GetString(self, index):
        return self.str_sec_data.GetData().GetString(err, index)

class DOFVarTabSection(DOFSection):
    def __init__(self, dof, target, data, i):
        super(DOFVarTabSection, self).__init__(dof, target, data, i)

        dtrace_difv_type = target.FindFirstType("dtrace_difv_t")
        if not(dtrace_difv_type):
            raise TypeError('Could not find dtrace_difv_t. Do you have a kernel binary loaded ?)')
        self.var_len = self.data_size // dtrace_difv_type.GetByteSize()
        # See note in DOFDIFSection.__init__
        target.EvaluateExpression("dtrace_difv_t (*$difva{})[{}]".format(self.var_len, self.var_len))
        difva_type = target.EvaluateExpression("$difva{}".format(self.var_len)).GetType().GetPointeeType()
        self.vartab = data.GetValue("$vartab", self.offset, difva_type)

    def GetVar(self, i):
        return self.vartab.GetChildAtIndex(i)


class DOFIntTabSection(DOFSection):
    def __init__(self, dof, target, data, i):
        super(DOFIntTabSection, self).__init__(dof, target, data, i)

        uint64_type = target.FindFirstType("uint64_t")
        if not(uint64_type):
            raise TypeError('Could not find uint64_t. Do you have a kernel binary loaded ?)')
        self.int_len = self.data_size // uint64_type.GetByteSize()
        # Ridiculous hack here : We don't have SBType::GetArrayType because
        # our lldb is too old, so we declare a variable of type
        # pointer to char[size_of_strtab] and we grab its Pointee type
        # (Note that we can't directly declare a variable of type char[size]
        # since lldb tends to truncate the array size of locals when debugging
        # remote kernels
        target.EvaluateExpression("uint64_t (*$intat)[{}]".format(self.int_len))
        inta_type = target.EvaluateExpression("$intat").GetType().GetPointeeType()
        self.inttab = data.GetValue("$inttab", self.offset, inta_type)

    def GetInt(self, i):
        return GetChildAtIndexAsUnsigned(self.inttab, i)

class DOFProbe(object):
    def __init__(self, func, name, nargv, xargv):
        self.func = func
        self.name = name
        self.nargv = nargv
        self.xargv = xargv

    def Show(self):
        if self.nargv != self.xargv:
            print('\t{}::{}({} | {})'.format(self.func, self.name, self.nargv,
                    self.xargv))
        else:
            print('\t{}::{}({})'.format(self.func, self.name, self.nargv))


class DOFProviderSection(DOFSection):
    """
        Helper (USDT) provider section, containing the definition of a provider
        and a link to a probe section containing the definition of probes for
        this provider. (usually) only contained in DOFs in files
    """
    def __init__(self, dof, target, data, i):
        super(DOFProviderSection, self).__init__(dof, target, data, i)
        # Find the dof_provider_t/dof_probe_t type
        dof_provider_type = target.FindFirstType("dof_provider_t")
        if not(dof_provider_type):
            raise TypeError('Could not find dof_provider_t. Do you have a kernel binary loaded ?)')
        dof_probe_type = target.FindFirstType("dof_probe_t")
        if not(dof_probe_type):
            raise TypeError('Could not find dof_probe_t. Do you have a kernel binary loaded ?)')
        # Retrieve the provider data from the section
        provider = data.GetValue("$provider", self.offset, dof_provider_type)
        self.strtab_secid = GetMemberAsSigned(provider, "dofpv_strtab")
        self.probes_secid = GetMemberAsSigned(provider, "dofpv_probes")
        self.prargs_secid = GetMemberAsSigned(provider, "dofpv_prargs")
        self.proffs_secid = GetMemberAsSigned(provider, "dofpv_proffs")
        self.name_idx = GetMemberAsUnsigned(provider, "dofpv_name")
        # Retrieve the strtab section to get strings
        str_sec = dof.GetSection(self.strtab_secid)
        # Retrieve the provider name
        self.provider_name = str_sec.GetString(self.name_idx)
        # Retrieve the probes associated with this provider
        # Since those depend on the location of the string table they can't be
        # retrieved from inside the PROBE section
        prb_sec = dof.GetSection(self.probes_secid)
        arg_sec = dof.GetSection(self.prargs_secid)
        off_sec = dof.GetSection(self.proffs_secid)

        self.nprobes = prb_sec.data_size // prb_sec.ent_size
        self.probes = []
        for i in range(self.nprobes):
            probe = data.GetValue("$probe", prb_sec.offset + i *
                    prb_sec.ent_size, dof_probe_type)
            probe_name_idx = GetMemberAsUnsigned(probe, "dofpr_name")
            probe_name = str_sec.GetString(probe_name_idx)
            probe_func_idx = GetMemberAsUnsigned(probe, "dofpr_func")
            probe_func = str_sec.GetString(probe_func_idx)
            probe_nargv_idx = GetMemberAsUnsigned(probe, "dofpr_nargv")

            # When nargv_idx / xargv_idx == probe_func_idx, the probe has no
            # arguments
            probe_nargv = str_sec.GetString(probe_nargv_idx) if probe_nargv_idx != probe_func_idx else "void"
            probe_xargv_idx = GetMemberAsUnsigned(probe, "dofpr_xargv")
            probe_xargv = str_sec.GetString(probe_xargv_idx) if probe_xargv_idx != probe_func_idx else "void"
            self.probes.append(DOFProbe(probe_func, probe_name, probe_nargv,
                probe_xargv))

    def Show(self):
        super(DOFProviderSection, self).Show()
        print('Provider name: {} ({} probes)'.format(self.provider_name,
                self.nprobes))
        for i in range(self.nprobes):
            self.probes[i].Show()


class DOFEcbDescSection(DOFSection):
    """
        ECB (Enabling control block) description section
    """
    def __init__(self, dof, target, data, i):
        super(DOFEcbDescSection, self).__init__(dof, target, data, i)

        # Find the dof_ecbdesc_t type
        dof_ecbdesc_type = target.FindFirstType("dof_ecbdesc_t")
        if not(dof_ecbdesc_type):
            raise TypeError('Could not find dof_ecbdesc_t. Do you have a kernel binary loaded ?')

        ecbdesc = data.GetValue("$ecbdesc", self.offset, dof_ecbdesc_type)

        self.probes_secid = GetMemberAsSigned(ecbdesc, "dofe_probes")
        self.pred_secid = GetMemberAsSigned(ecbdesc, "dofe_pred")
        self.actions_secid = GetMemberAsSigned(ecbdesc, "dofe_actions")

    def Show(self):
        super(DOFEcbDescSection, self).Show()
        print('\tprobe {} pred {} actions {}'.format(
            DOFSecIdxString(self.probes_secid),
            DOFSecIdxString(self.pred_secid), 
            DOFSecIdxString(self.actions_secid)
        ))


class DOFProbeDescSection(DOFSection):
    """
        Probe description section
    """
    def __init__(self, dof, target, data, i):
        super(DOFProbeDescSection, self).__init__(dof, target, data, i)
        # Find the dof_probedesc_t type
        dof_probedesc_type = target.FindFirstType("dof_probedesc_t")
        if not(dof_probedesc_type):
            raise TypeError('Could not find dof_probedesc_t. Do you have a kernel binary loaded ?')

        probe = data.GetValue("$probe", self.offset, dof_probedesc_type)
        str_sec_secid = GetMemberAsSigned(probe, "dofp_strtab")
        str_sec = dof.GetSection(str_sec_secid)

        provider_idx = GetMemberAsUnsigned(probe, "dofp_provider")
        mod_idx = GetMemberAsUnsigned(probe, "dofp_mod")
        func_idx = GetMemberAsUnsigned(probe, "dofp_func")
        name_idx = GetMemberAsUnsigned(probe, "dofp_name")

        self.provider = str_sec.GetString(provider_idx)
        self.mod = str_sec.GetString(mod_idx)
        self.func = str_sec.GetString(func_idx)
        self.name = str_sec.GetString(name_idx)

    def Show(self):
        super(DOFProbeDescSection, self).Show()
        print('\tProbe desc {}:{}:{}:{}'.format(self.provider,
                self.mod, self.func, self.name))


class DOFAction(object):
    def __init__(self, i, kind, difo, arg, arg_string):
        self.i = i
        self.kind = kind
        self.difo = difo
        self.arg = arg
        self.arg_string = arg_string


class DOFActDescSection(DOFSection):
    """
        Probe action description. The section contains multiple actions
    """
    def __init__(self, dof, target, data, i):
        super(DOFActDescSection, self).__init__(dof, target, data, i)
        # Find the DOF types
        dof_actdesc_type = target.FindFirstType("dof_actdesc_t")
        if not(dof_actdesc_type.IsValid()):
            raise TypeError('Could not find dof_actdesc_t type. Do you have a kenrel binary loaded ?')

        # Retrieve the number of actions
        nactions = self.data_size // self.ent_size
        self.actions = []
        for i in range(nactions):
            act = data.GetValue("$act", self.ent_size * i + self.offset, dof_actdesc_type)
            kind = GetMemberAsUnsigned(act, "dofa_kind")
            difo = GetMemberAsSigned(act, "dofa_difo")
            strtab_secid = GetMemberAsSigned(act, "dofa_strtab")
            arg = GetMemberAsUnsigned(act, "dofa_arg")
            arg_string = None
            if (kind in [DTRACEACT_PRINTF, DTRACEACT_PRINTA, DTRACEACT_SYSTEM,
                DTRACEACT_FREOPEN]) and ((kind != DTRACEACT_PRINTA or strtab_secid != -1) or (kind == DTRACEACT_DIFEXPR and strtab_secid != -1)):
                strtab = dof.GetSection(strtab_secid)
                arg_string = strtab.GetString(arg)

            # TODO : Show the argument for print/printf/... actions
            self.actions.append(DOFAction(i, kind, difo, arg, arg_string))

    def Show(self):
        super(DOFActDescSection, self).Show()
        for a in self.actions:
            print('\tAction {} type {} difo in {}'.format(
                a.i,
                ActionString(a.kind),
                a.difo,
            ), end=' ')
            if a.arg_string is not(None):
                print('arg "{}"'.format(a.arg_string))
            else:
                print('arg {}'.format(a.arg))


class DOFDifoHdrSection(DOFSection):
    """
        DIFO header section. Contains references to
        DIF/INTTAB/STRTAB/VARTAB sections
    """
    def __init__(self, dof, target, data, i):
        super(DOFDifoHdrSection, self).__init__(dof, target, data, i)
        # Retrieve required types
        dof_difohdr_type = target.FindFirstType("dof_difohdr_t")
        if not(dof_difohdr_type.IsValid()):
            raise TypeError('Could not find dof_difohdr_t type. Do you have a kernel binary loaded ?')
        dof_secidx_type = target.FindFirstType("dof_secidx_t")
        if not(dof_secidx_type.IsValid()):
            raise TypeError('Could not find dof_secidx_t type. Do you have a kernel binary loaded ?')

        self.nsections = (self.data_size - dof_difohdr_type.GetByteSize()) // dof_secidx_type.GetByteSize() + 1

        self.secs = []
        (self.dif_idx, self.dif) = (0, None)
        (self.inttab_idx, self.inttab) = (0, None)
        self.strtab_idx, self.strtab = (0, None)
        (self.vartab_idx, self.vartab) = (0, None)

        for i in range(self.nsections):
            sec_idx = data.GetValue("$idx", self.offset + dof_difohdr_type.GetByteSize()
                    + (i - 1) * dof_secidx_type.GetByteSize(),
                    dof_secidx_type).GetValueAsSigned()
            sec = dof.GetSection(sec_idx)
            if sec.stype == "DIF":
                self.dif_idx = sec_idx
                self.dif = sec
            elif sec.stype == "STRTAB":
                self.strtab_idx = sec_idx
                self.strtab = sec
            elif sec.stype == "INTTAB":
                self.inttab_idx = sec_idx
                self.inttab = sec
            elif sec.stype == "VARTAB":
                self.vartab_idx = sec_idx
                self.vartab = sec
            self.secs.append(sec_idx)

        self.disassembler = DIFODisassembler(self.dif, self.strtab,
                self.inttab, self.vartab)

    def Show(self):
        super(DOFDifoHdrSection, self).Show()
        print('DIF {} STRTAB {} INTTAB {} VARTAB {}'.format(
            DOFSecIdxString(self.dif_idx),
            DOFSecIdxString(self.strtab_idx),
            DOFSecIdxString(self.inttab_idx),
            DOFSecIdxString(self.vartab_idx)
        ))
        self.disassembler.Disassemble()


# Section classes, ordered by type number
section_classes = {
    'ECBDESC': DOFEcbDescSection,
    'PROBEDESC': DOFProbeDescSection,
    'ACTDESC': DOFActDescSection,
    'DIFOHDR': DOFDifoHdrSection,
    'DIF': DOFDIFSection,
    'STRTAB': DOFStrTabSection,
    'VARTAB': DOFVarTabSection,
    'PROVIDER': DOFProviderSection,
    'INTTAB': DOFIntTabSection,
}

class DOF(object):
    def __init__(self, target, data):
        self.target = target
        self.data = data
        self.addr = data.GetAddr()
        # Find the DOF types
        self.dof_sec_type = target.FindFirstType("dof_sec_t")
        if not(self.dof_sec_type.IsValid()):
            raise TypeError('Could not find dof_sec_t type. Do you have a kernel binary loaded ?')
            return
        # Retrieve the header
        self.header = DOFHeader(self, target, data)
        # Populate the section dictionary
        self.sections = {}
        for i in range(self.header.nsecs):
            section = self.GetSection(i)

    def GetSection(self, index):
        """Retrieves a section from the DOF, parsing it if it hasn't been
        already"""
        if index not in self.sections:
            stype = self.GetSectionType(index)
            # Create a different child of DOFSection depending on the section
            # type
            if stype in section_classes:
                SectionType = section_classes[stype]
            else:
                # If we have no specific class for this section type, just
                # retrieve the section header
                SectionType = DOFSection
            self.sections[index] = SectionType(self, self.target, self.data, index)
        return self.sections[index]

    def Show(self):
        self.header.Show()
        print('Sections:')
        for i in range(len(self.sections)):
            self.sections[i].Show()

    def GetSectionType(self, i):
        sec = self.data.GetValue("$sec", self.header.secoff +
                self.header.secsize * i, self.dof_sec_type)
        return section_names[GetMemberAsUnsigned(sec, "dofs_type")]


def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand('command script add -f dof.dof_command dof')

def show_dof_module(target, module):
    print('Printing DOFs for module {}'.format(module.GetFileSpec().GetFilename()))
    text_section = module.FindSection("__TEXT")
    # Iterate over the sections of TEXT to find the dof ones
    for i in range(text_section.GetNumSubSections()):
        section = text_section.GetSubSectionAtIndex(i)
        if section.GetName().find("dof_") != -1:
            section_data = SectionData(target, section)
            dof = DOF(target, section_data)
            print('Section {}'.format(section.GetName()))
            dof.Show()


def dof_command_section(debugger, args, result, internal_dict):
    """
        Retrieves DOF data from DOF sections inside a specific module
    """
    target = debugger.GetSelectedTarget()

    # If we got no arguments, just assume we are looking at the first module
    if len(args) == 1:
        return show_dof_module(target, target.GetModuleAtIndex(0))

    subsubcommand = args[1]
    if subsubcommand == 'index':
        i = int(args[2])
        return show_dof_module(target, target.GetModuleAtIndex(i))
    elif subsubcommand == 'file':
        filename = args[2]
        filespec = lldb.SBFileSpec(filename)
        return show_dof_module(target, target.FindModule(filespec))
    elif subsubcommand == 'all':
        for i in range(target.GetNumModules()):
            show_dof_module(target, target.GetModuleAtIndex(i))

def dof_command_addr(debugger, args, result, internal_dict):
    """
        Parses DOF from an address
    """
    target = debugger.GetSelectedTarget()
    dof_hdr_type = target.FindFirstType("dof_hdr_t")

    if len(args) == 1:
        return
    addr = int(args[1], 0)

    addr_data = AddrData(target, addr)
    dof = DOF(target, addr_data)
    dof.Show()

def dof_command(debugger, command, result, internal_dict):

    args = shlex.split(command)
    if len(args) == 0:
        return

    subcommand = args[0]
    if subcommand == "section":
        dof_command_section(debugger, args, result, internal_dict)
    elif subcommand == "addr":
        dof_command_addr(debugger, args, result, internal_dict)
