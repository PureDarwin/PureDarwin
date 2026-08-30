""" Please make sure you read the README COMPLETELY BEFORE reading anything below.
    It is very critical that you read coding guidelines in Section E in README file.
"""
from xnu import *
from utils import *
from string import *
from socket import *
from enum import IntEnum

import xnudefines
from netdefines import *
from routedefines import *

######################################
# Globals
######################################
""" Refer to in bsd/net/ntstat.c
"""
NSTAT_PROCDETAILS_MAGIC = 0xfeedc001
NSTAT_GENERIC_SHADOW_MAGIC = 0xfadef00d
TU_SHADOW_MAGIC = 0xfeedf00d
""" Refer to nstat_provider_type_t in bsd/net/ntstat.h
"""
class NSTAT_PROVIDER(IntEnum):
    NONE = 0
    ROUTE = 1
    TCP_KERNEL = 2
    TCP_USERLAND = 3
    UDP_KERNEL = 4
    UDP_USERLAND = 5
    IFNET = 6
    SYSINFO = 7
    QUIC_USERLAND = 8
    CONN_USERLAND = 9
    UDP_SUBFLOW = 10

######################################
# Helper functions
######################################

def FieldPtrToStructPtr(field_ptr,  field_name, element_type):
    """ Given a pointer to a field with a structure, return a pointer to the structure itself
    params:
        field_ptr       - value : pointer to the field
        field_name      - str   : string name of the field which holds the list links.
        element_type    - str   : type of elements to be linked in the list
    returns:
        value : A pointer to the start of the structure
    """
    out_string = ""
    if (field_ptr) :
        tmp_element = Cast(field_ptr, element_type)
        tmp_element_plus_offset = addressof(tmp_element.__getattr__(field_name))
        offset = tmp_element_plus_offset - tmp_element
        original_ptr_as_char_ptr = Cast(field_ptr, 'char *')
        amended_ptr = original_ptr_as_char_ptr - offset
        return kern.GetValueFromAddress(unsigned(amended_ptr), element_type)
    return field_ptr

def ReverseIterateTAILQ_AnonymousHEAD(headval, field_name, element_type):
    """ reverse iterate over a TAILQ_HEAD in kernel. refer to bsd/sys/queue.h
    params:
        headval         - value : value object representing the head of the list
        field_name      - str   : string name of the field which holds the list links.
        element_type    - str   : type of elements to be linked in the list
    returns:
        A generator does not return. It is used for iterating.
        value : an object that is of type as headval->tqh_last. Always a pointer object
    example usage:
        list_head = kern.GetGlobalVariable('ctl_head')
        for entry in ReverseIterateTAILQ_AnonymousHEAD(list_head, 'next', 'struct kctl *'):
            print(entry)
    """
    head_first = headval.__getattr__('tqh_first')
    if head_first:
        head_first_addr = FieldPtrToStructPtr(addressof(head_first),field_name, element_type)
        head_last = headval.__getattr__('tqh_last')
        iter_val = FieldPtrToStructPtr(head_last, field_name, element_type)

        while (unsigned(iter_val) != unsigned(head_first_addr)) and (unsigned(iter_val) != 0) :
            yield iter_val
            element = Cast(iter_val, element_type)
            tmp  = element.__getattr__(field_name).__getattr__('tqe_prev')
            iter_val = FieldPtrToStructPtr(tmp, field_name, element_type)
        #end of yield loop

def ShowNstatTUShadow(inshadow):
    """ Display summary for an nstat_tu_shadow struct
        params:
            inshadow : cvalue object which points to 'struct nstat_tu_shadow *'
    """
    shad = Cast(inshadow, 'struct nstat_tu_shadow *')
    procdetails = shad.shad_procdetails
    out_string = ""
    if shad :
        format_string = "nstat_tu_shadow {0: <#0x}: next={1: <#020x} prev={2: <#020x} context (necp_client *)={3: <#020x} live={4: <d}"
        out_string += format_string.format(shad, shad.shad_link.tqe_next, shad.shad_link.tqe_prev, shad.shad_provider_context, shad.shad_live)

        magic = unsigned(shad.shad_magic)
        if (magic != TU_SHADOW_MAGIC) :
            format_string = " INVALID shad magic {0: <#0x}"
            out_string += format_string.format(magic)

        if (procdetails) :
            format_string = "  --> procdetails {0: <#0x}: pid={1: <d} name={2: <s} refcnt={3: <d}"
            out_string += format_string.format(procdetails, procdetails.pdet_pid, procdetails.pdet_procname, procdetails.pdet_refcnt)

            procmagic = unsigned(procdetails.pdet_magic)
            if (procmagic != NSTAT_PROCDETAILS_MAGIC) :
                format_string = " INVALID proc magic {0: <#0x}"
                out_string += format_string.format(procmagic)

    print(out_string)

def ShowNstatGShadow(inshadow):
    """ Display summary for an nstat_generic_shadow
        params:
            inshadow : cvalue object which points to 'struct nstat_generic_shadow *'
    """
    gshad = Cast(inshadow, 'struct nstat_generic_shadow *')
    procdetails = gshad.gshad_procdetails
    out_string = ""
    if gshad :
        prov_string = GetNstatProviderString(gshad.gshad_provider)
        format_string = "nstat_generic_shadow {0: <#0x}: prov={1: <8s} next={2: <#020x} prev={3: <#020x} refcnt={4: <d} "
        out_string += format_string.format(gshad, prov_string, gshad.gshad_link.tqe_next, gshad.gshad_link.tqe_prev, gshad.gshad_refcnt)

        ## context
        if (gshad.gshad_provider == NSTAT_PROVIDER.CONN_USERLAND) :
            out_string += "context (necp_client *)={0: <#020x} ".format(gshad.gshad_provider_context)
        elif (gshad.gshad_provider == NSTAT_PROVIDER.UDP_SUBFLOW) :
            out_string += "context (soflow_hash_entry *)={0: <#020x} ".format(gshad.gshad_provider_context)

        magic = unsigned(gshad.gshad_magic)
        if (magic != NSTAT_GENERIC_SHADOW_MAGIC) :
            format_string = " INVALID gshad magic {0: <#0x}"
            out_string += format_string.format(magic)

        if (procdetails) :
            format_string = " --> procdetails {0: <#0x}: pid={1: <d} name={2: <s} refcnt={3: <d}"
            out_string += format_string.format(procdetails, procdetails.pdet_pid, procdetails.pdet_procname, procdetails.pdet_refcnt)

            procmagic = unsigned(procdetails.pdet_magic)
            if (procmagic != NSTAT_PROCDETAILS_MAGIC) :
                format_string = " INVALID proc magic {0: <#0x}"
                out_string += format_string.format(procmagic)

        print(out_string)

        for src in IterateTAILQ_HEAD(gshad.gshad_locus.ntl_src_queue, 'nts_locus_link'):
            ShowNstatSrc(src)
    else:
        print(out_string)

def GetNstatProcdetailsBrief(procdetails):
    """ Display a brief summary for an nstat_procdetails struct
        params:
            procdetails : cvalue object which points to 'struct nstat_procdetails *'
        returns:
            str : A string describing various information for the nstat_procdetails structure
    """
    procdetails = Cast(procdetails, 'struct nstat_procdetails *')
    out_string = ""
    if (procdetails) :
        format_string = " --> pid={0: <d} name={1: <s} refcnt={2: <d}"
        out_string += format_string.format(procdetails.pdet_pid, procdetails.pdet_procname, procdetails.pdet_refcnt)

        procmagic = unsigned(procdetails.pdet_magic)
        if (procmagic != NSTAT_PROCDETAILS_MAGIC) :
            format_string = " INVALID proc magic {0: <#0x}"
            out_string += format_string.format(procmagic)

    return out_string

def ShowNstatProcdetails(procdetails):
    """ Display a summary for an nstat_procdetails struct
        params:
            procdetails : cvalue object which points to 'struct nstat_procdetails *'
    """
    procdetails = Cast(procdetails, 'struct nstat_procdetails *')
    out_string = ""
    if (procdetails) :
        format_string = "nstat_procdetails: {0: <#020x} next={1: <#020x} prev={2: <#020x} "
        out_string += format_string.format(procdetails, procdetails.pdet_link.tqe_next, procdetails.pdet_link.tqe_prev)
        out_string += GetNstatProcdetailsBrief(procdetails)

    print(out_string)

def ShowNstatSockLocus(locus):
    """ Display a summary for an nstat_sock_locus struct
        params:
            locus : cvalue object which points to 'struct nstat_sock_locus *'
    """
    locus = Cast(locus, 'struct nstat_sock_locus *')
    out_string = ""
    if (locus) :
        format_string = "nstat_sock_locus: {0: <#020x} next={1: <#020x} prev={2: <#020x}"
        out_string += format_string.format(locus, locus.nsl_link.tqe_next, locus.nsl_link.tqe_prev)
        out_string += GetNstatTULocusBrief(locus);

    print(out_string)
    iterator = IterateTAILQ_HEAD(locus.nsl_locus.ntl_src_queue, 'nts_locus_link')
    for src in iterator:
        ShowNstatSrc(src)


def GetNstatTUShadowBrief(shadow):
    """ Display a summary for an nstat_tu_shadow struct
        params:
            shadow : cvalue object which points to 'struct nstat_tu_shadow *'
        returns:
            str : A string describing various information for the nstat_tu_shadow structure
    """
    out_string = ""
    shad = Cast(shadow, 'struct nstat_tu_shadow *')
    procdetails = shad.shad_procdetails
    procdetails = Cast(procdetails, 'struct nstat_procdetails *')
    out_string = ""
    if shad :
        format_string = " shadow {0: <#0x}: necp_client={1: <#020x} live={2: <d}"
        out_string += format_string.format(shad, shad.shad_provider_context, shad.shad_live)
        magic = unsigned(shad.shad_magic)
        if (magic != TU_SHADOW_MAGIC) :
            format_string = " INVALID shad magic {0: <#0x}"
            out_string += format_string.format(magic)
        elif (procdetails) :
            out_string += GetNstatProcdetailsBrief(procdetails)

    return out_string

def GetNstatGenericShadowBrief(shadow):
    """ Display a summary for an nstat_generic_shadow struct
        params:
            shadow : cvalue object which points to 'struct nstat_generic_shadow *'
        returns:
            str : A string describing various information for the nstat_tu_shadow structure
    """
    gshad = Cast(shadow, 'struct nstat_generic_shadow *')
    procdetails = gshad.gshad_procdetails
    procdetails = Cast(procdetails, 'struct nstat_procdetails *')
    out_string = ""
    if gshad :
        format_string = " gshadow {0: <#0x}:"
        out_string += format_string.format(gshad)
        if (gshad.gshad_provider == NSTAT_PROVIDER.CONN_USERLAND) :
            out_string += "necp_client={0: <#020x} ".format(gshad.gshad_provider_context)
        elif (gshad.gshad_provider == NSTAT_PROVIDER.UDP_SUBFLOW) :
            out_string += "soflow_hash_entry={0: <#020x} ".format(gshad.gshad_provider_context)
        else :
            out_string += "context {0: <#020x} ".format(gshad.gshad_provider_context)
        out_string += " refcnt={0: <d} ".format(gshad.gshad_refcnt)

        magic = unsigned(gshad.gshad_magic)
        if (magic != NSTAT_GENERIC_SHADOW_MAGIC) :
            format_string = " INVALID gshad magic {0: <#0x}"
            out_string += format_string.format(magic)
        elif (procdetails) :
            out_string += GetNstatProcdetailsBrief(procdetails)

    return out_string

def GetNstatTULocusBrief(cookie):
    """ Display a summary for an nnstat_sock_locus struct
        params:
            cookie : cvalue object which points to 'struct nstat_sock_locus *'
        returns:
            str : A string describing various information for the nstat_sock_locus structure
    """
    out_string = ""
    sol = Cast(cookie, 'struct nstat_sock_locus *')
    inp = sol.nsl_inp
    inpcb = Cast(inp, 'struct inpcb *')
    inp_socket = inpcb.inp_socket
    sock = Cast(inp_socket, 'struct socket *')
    pname = sol.nsl_pname
    format_string = " inpcb={0: <#0x}: socket={1: <#020x} process={2: <s}"
    out_string += format_string.format(inpcb, sock, pname)
    return out_string

def GetNstatProviderString(provider):
    providers = {
        NSTAT_PROVIDER.NONE: "none",
        NSTAT_PROVIDER.ROUTE: "route",
        NSTAT_PROVIDER.TCP_KERNEL: "TCP k",
        NSTAT_PROVIDER.TCP_USERLAND: "TCP u",
        NSTAT_PROVIDER.UDP_KERNEL: "UDP k",
        NSTAT_PROVIDER.UDP_USERLAND: "UDP u",
        NSTAT_PROVIDER.IFNET: "ifnet",
        NSTAT_PROVIDER.SYSINFO: "sysinfo",
        NSTAT_PROVIDER.QUIC_USERLAND: "quic u",
        NSTAT_PROVIDER.CONN_USERLAND: "conn u",
        NSTAT_PROVIDER.UDP_SUBFLOW: "subflow",
    }
    return providers.get(unsigned(provider), "unknown")

def ShowNstatSrc(insrc):
    """ Display summary for an nstat_src struct
        params:
            insrc : cvalue object which points to 'struct nstat_src *'
    """
    src = Cast(insrc, 'nstat_src *')
    prov = src.nts_provider
    prov = Cast(prov, 'nstat_provider *')
    prov_string = GetNstatProviderString(prov.nstat_provider_id)
    out_string = ""
    if src :
        format_string = "  nstat_src {0: <#0x}: prov={1: <8s} next={2: <#020x} prev={3: <#020x} srcref={4: <d} seq={5: <d}"
        out_string += format_string.format(src, prov_string, src.nts_client_link.tqe_next, src.nts_client_link.tqe_prev, src.nts_srcref, src.nts_seq)

        if ((prov.nstat_provider_id == NSTAT_PROVIDER.TCP_USERLAND) or
            (prov.nstat_provider_id == NSTAT_PROVIDER.UDP_USERLAND) or
            (prov.nstat_provider_id == NSTAT_PROVIDER.QUIC_USERLAND)) :
            out_string += GetNstatTUShadowBrief(src.nts_cookie);
        elif ((prov.nstat_provider_id == NSTAT_PROVIDER.CONN_USERLAND) or
            (prov.nstat_provider_id == NSTAT_PROVIDER.UDP_SUBFLOW)) :
            out_string += GetNstatGenericShadowBrief(src.nts_cookie);
        elif ((prov.nstat_provider_id == NSTAT_PROVIDER.TCP_KERNEL) or
            (prov.nstat_provider_id == NSTAT_PROVIDER.UDP_KERNEL)) :
            out_string += GetNstatTULocusBrief(src.nts_cookie);

    print(out_string)

def ShowNstatClient(inclient, reverse):
    """ Display an nstat_client struct
        params:
            client : value object representing an nstat_client in the kernel
    """
    client = Cast(inclient, 'nstat_client *')
    out_string = ""
    if client :
        format_string = "\nnstat_client {0: <#0x}: next={1: <#020x} src-head={2: <#020x} tail={3: <#020x}"
        out_string += format_string.format(client, client.ntc_next, client.ntc_src_queue.tqh_first, client.ntc_src_queue.tqh_last)
        procdetails = client.ntc_procdetails
        if (procdetails) :
            format_string = "  --> procdetails {0: <#0x}: pid={1: <d} name={2: <s} refcnt={3: <d}"
            out_string += format_string.format(procdetails, procdetails.pdet_pid, procdetails.pdet_procname, procdetails.pdet_refcnt)

    print(out_string)
    if reverse:
         print("reverse nstat_src list:")
         iterator = ReverseIterateTAILQ_AnonymousHEAD(client.ntc_src_queue, 'nts_client_link', 'struct nstat_src *')
    else:
         print("nstat_src list:")
         iterator = IterateTAILQ_HEAD(client.ntc_src_queue, 'nts_client_link')
    for src in iterator:
        ShowNstatSrc(src)

######################################
# Print functions
######################################
def PrintNstatClientList(reverse):
    print("nstat_clients list:")
    client = kern.globals.nstat_clients
    client = cast(client, 'nstat_client *')
    while client != 0:
        ShowNstatClient(client, reverse)
        client = cast(client.ntc_next, 'nstat_client *')

def PrintNstatProcdetailList(reverse):
    procdetails_head = kern.globals.nstat_procdetails_head
    if reverse:
        print("\nreverse nstat_procdetails list:\n")
        iterator = ReverseIterateTAILQ_AnonymousHEAD(procdetails_head, 'pdet_link', 'struct nstat_procdetails *')
    else:
        print("\nnstat_procdetails list:\n")
        iterator = IterateTAILQ_HEAD(procdetails_head, 'pdet_link')
    for procdetails in iterator:
        ShowNstatProcdetails(procdetails)

def PrintNstatGenericShadowList(reverse):
    gshadows = kern.globals.nstat_gshad_head
    if reverse:
        print("\nreverse nstat_ghsad list:\n")
        iterator = ReverseIterateTAILQ_AnonymousHEAD(gshadows, 'gshad_link', 'struct nstat_generic_shadow *')
    else:
        print("\nnstat_ghsad list:\n")
        iterator = IterateTAILQ_HEAD(gshadows, 'gshad_link')
    for gshad in iterator:
        ShowNstatGShadow(gshad)

def PrintNstatTUShadowList(reverse):
    shadows = kern.globals.nstat_userprot_shad_head
    if reverse:
        print("\nreverse nstat_userprot_shad list:\n")
        iterator = ReverseIterateTAILQ_AnonymousHEAD(shadows, 'shad_link', 'struct nstat_tu_shadow *')
    else:
        print("\nnstat_userprot_shad list:\n")
        iterator = IterateTAILQ_HEAD(shadows, 'shad_link')
    for shad in iterator:
        ShowNstatTUShadow(shad)

def PrintNstatTCPLocusList(reverse):
    loci = kern.globals.nstat_tcp_sock_locus_head
    if reverse:
        print("\nreverse nstat tcp socket locus list:\n")
        iterator = ReverseIterateTAILQ_AnonymousHEAD(loci, 'nsl_link', 'struct nstat_sock_locus *')
    else:
        print("\nnstat tcp socket locus list:\n")
        iterator = IterateTAILQ_HEAD(loci, 'nsl_link')
    for locus in iterator:
        ShowNstatSockLocus(locus)

def PrintNstatUDPLocusList(reverse):
    loci = kern.globals.nstat_udp_sock_locus_head
    if reverse:
        print("\nreverse nstat udp socket locus list:\n")
        iterator = ReverseIterateTAILQ_AnonymousHEAD(loci, 'nsl_link', 'struct nstat_sock_locus *')
    else:
        print("\nnstat udp socket locus list:\n")
        iterator = IterateTAILQ_HEAD(loci, 'nsl_link')
    for locus in iterator:
        ShowNstatSockLocus(locus)

######################################
# LLDB commands
######################################
# Macro: showallntstat

@lldb_command('showallntstat', 'R')
def ShowAllNtstat(cmd_args=None, cmd_options={}) :
    """ Show the contents of various ntstat (network statistics) data structures

        usage: showallntstat [-R]
            -R  : print ntstat list in reverse
    """
    reverse = '-R' in cmd_options

    PrintNstatClientList(reverse)
    PrintNstatTUShadowList(reverse)
    PrintNstatGenericShadowList(reverse)
    PrintNstatProcdetailList(reverse)
    PrintNstatTCPLocusList(reverse)
    PrintNstatUDPLocusList(reverse)

# EndMacro: showallntstat
