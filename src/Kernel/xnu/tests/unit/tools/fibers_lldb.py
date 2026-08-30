#!/usr/bin/env python3
'''
How to Use:

Load in LLDB:
(lldb) command script import ./tests/unit/tools/fibers_lldb.py

Run the commands:
(lldb) fibers_all         # Lists all existing fibers
(lldb) fibers_ready       # Lists fibers in the run queue
(lldb) fibers_current     # Gets information about the current fiber
(lldb) fibers_regs [id]   # Get the registers saved in the fiber end (default current fiber)
'''

import lldb
import sys

def fiber_state_to_string(state):
    """Converts a fiber state integer to a human-readable string."""
    states = []
    if state & 0x1:
        states.append("RUN")
    if state & 0x2:
        states.append("STOP")
    if state & 0x4:
        states.append("WAIT")
    if state & 0x8:
        states.append("JOIN")
    if state & 0x10:
        states.append("DEAD")
    return "|".join(states) if states else "UNKNOWN"

def strip_pointer(target, addr):
    """Strips the PAC signature from a pointer."""
    val = target.CreateValueFromAddress("__tmp_strip_pac", lldb.SBAddress(addr, target), target.FindFirstType("unsigned long long"))
    val.SetPreferDynamicValue(lldb.eNoDynamicValues)
    val = val.AddressOf()
    return val.GetValueAsAddress()

def strip_fp_lr_sp(process, target, fp, lr, sp):
    """Strip manged registers in the jmp buf from the munge token and PAC."""
    # get the munge token (see __longjmp impl)
    frame = process.selected_thread.GetFrameAtIndex(0)

    # ref. os/tsd.h
    # define __TSD_PTR_MUNGE 7
    munge_token = frame.EvaluateExpression('({void** r; __asm__("mrs %0, TPIDRRO_EL0" : "=r"(r)); r[7];})')
    if munge_token.GetError().Fail():
        return None
    munge_token = munge_token.GetValueAsAddress()

    fp  = strip_pointer(target, fp ^ munge_token)
    lr  = strip_pointer(target, lr ^ munge_token)
    sp  = strip_pointer(target, sp ^ munge_token)
    return (fp, lr, sp)

def get_fiber_info(debugger, fiber_value):
    """Retrieves information about a fiber from its SBValue address."""
    if not fiber_value or not fiber_value.IsValid():
        return None

    fiber_address = fiber_value.GetValueAsAddress()

    fiber_id_value = fiber_value.GetChildMemberWithName('id')
    fiber_id_state = fiber_value.GetChildMemberWithName('state')
    stack_bottom_value = fiber_value.GetChildMemberWithName('stack_bottom')
    env_value = fiber_value.GetChildMemberWithName('env')
    if not fiber_id_value.IsValid() or not fiber_id_state.IsValid() or not stack_bottom_value.IsValid() or not env_value.IsValid():
        print(f"Error reading fiber memory")
        return None

    fiber_id = fiber_id_value.GetValueAsUnsigned()
    fiber_state = fiber_id_state.GetValueAsUnsigned()
    stack_bottom = stack_bottom_value.GetValueAsAddress()
    env_address = env_value.AddressOf().GetValueAsAddress()

    return {
        "id": fiber_id,
        "address": fiber_address,
        "state": fiber_state,
        "state_str": fiber_state_to_string(fiber_state),
        "stack_bottom": stack_bottom,
        "env_address": env_address
    }

def print_stack_trace_from_jmp_buf(debugger, fiber_info, result, arch):
    """Prints a stack trace by manually walking the stack."""
    target = debugger.GetSelectedTarget()
    process = target.GetProcess()
    env_address = fiber_info["env_address"]
    error = lldb.SBError()
    addr_size = target.GetAddressByteSize()

    if arch == "x86_64":
        result.AppendMessage(f"  Error: Register printing is not supported on x86_64.")
        return

    elif arch == "arm64":
        FP_OFFSET = 80
        LR_OFFSET  = 88
        SP_OFFSET = 96

        fp  = process.ReadPointerFromMemory(env_address + FP_OFFSET, error)
        lr  = process.ReadPointerFromMemory(env_address + LR_OFFSET, error)
        sp  = process.ReadPointerFromMemory(env_address + SP_OFFSET, error)

        if error.Fail():
            result.AppendMessage(f"  Error: Could not read registers from jmp_buf: {error}")
            return
    
        strip_res = strip_fp_lr_sp(process, target, fp, lr, sp)
        if strip_res is None:
            result.AppendMessage(f"  Error: Could not strip FP LR or SP")
            return
        fp, lr, sp = strip_res

        result.AppendMessage(f"  Stack trace for fiber {fiber_info['id']} (manual backtrace):")

        for i in range(10):  # Limit to 10 frames for simplicity
            symbol_context = target.ResolveSymbolContextForAddress(lldb.SBAddress(lr, target), lldb.eSymbolContextEverything)
            symbol = symbol_context.GetSymbol()
            if symbol:
                symbol_name = symbol.GetName()
            else:
                symbol_name = "unknown"
            result.AppendMessage(f"    #{i}: 0x{lr:x} {symbol_name}")

            next_fp = process.ReadPointerFromMemory(fp, error)
            if error.Fail():
                result.AppendMessage(f"  Error: Could not read next FP from memory: {error}")
                break

            next_lr = process.ReadPointerFromMemory(fp + 8, error) # read next LR from the stack using current SP
            if error.Fail():
                result.AppendMessage(f"  Error: Could not read next LR from memory: {error}")
                break

            if next_lr == 0:
                result.AppendMessage("    End of stack or error reading memory.")
                break

            next_lr = strip_pointer(target, next_lr)
            lr = next_lr
            fp = next_fp

    else:
        result.AppendMessage(f"  Error: Unsupported architecture: {arch}")
        return


def list_fibers_all(debugger, command, result, internal_dict, arch):
    """Lists all existing fibers."""
    list_fibers_from_queue(debugger, command, result, internal_dict, "fibers_existing_queue", "All Existing Fibers", arch)

def list_fibers_ready(debugger, command, result, internal_dict, arch):
    """Lists fibers in the run queue (now called 'ready')."""
    list_fibers_from_queue(debugger, command, result, internal_dict, "fibers_run_queue", "Ready Fibers (Run Queue)", arch)

def list_fibers_from_queue(debugger, command, result, internal_dict, queue_name, title, arch):
    """Lists fibers from a specified queue."""

    target = debugger.GetSelectedTarget()

    queue_var = target.FindFirstGlobalVariable(queue_name)
    if not queue_var.IsValid():
        result.SetError(f"Could not find '{queue_name}' global variable.")
        return

    result.AppendMessage(f"{title}:")
    result.AppendMessage("-------")

    queue_top_value = queue_var.GetChildMemberWithName('top')
    if not queue_top_value.IsValid():
        result.SetError(f"Could not find '{queue_name}.top' field.")
        return

    fiber_value = queue_top_value
    while fiber_value and fiber_value.IsValid():
        fiber = get_fiber_info(debugger, fiber_value)
        if fiber:
            result.AppendMessage(f"  ID: {fiber['id']}, Address: 0x{fiber['address']:x}, State: {fiber['state_str']}, Stack Bottom: 0x{fiber['stack_bottom']:x}")
            try:
                print_stack_trace_from_jmp_buf(debugger, fiber, result, arch)  # Optional: Add stack traces
            except Exception as err:
                result.AppendMessage(f"Error: failed to get a stacktrace: {err}")
                break
        else:
            result.AppendMessage(f"Warning: Could not read fiber at address 0x{fiber_value.GetValueAsUnsigned():x}")
            break

        if queue_name == "fibers_existing_queue":
            next_fiber_value = fiber_value.GetChildMemberWithName('next_existing')
        else:
            next_fiber_value = fiber_value.GetChildMemberWithName('next')

        if not next_fiber_value.IsValid():
            break

        fiber_value = next_fiber_value

def get_current_fiber_info(debugger, command, result, internal_dict, arch):
    """Gets and prints information about the current fiber."""
    target = debugger.GetSelectedTarget()

    fibers_current_var = target.FindFirstGlobalVariable("fibers_current")
    if not fibers_current_var.IsValid():
        result.SetError("Could not find 'fibers_current' global variable.")
        return

    current_fiber = get_fiber_info(debugger, fibers_current_var)

    if not current_fiber:
        result.AppendMessage("No current fiber.")
        return

    result.AppendMessage("Current Fiber Information:")
    result.AppendMessage("--------------------------")
    result.AppendMessage(f"  ID: {current_fiber['id']}")
    result.AppendMessage(f"  Address: 0x{current_fiber['address']:x}")
    result.AppendMessage(f"  State: {current_fiber['state_str']}")
    result.AppendMessage(f"  Stack Bottom: 0x{current_fiber['stack_bottom']:x}")
    try:
        print_stack_trace_from_jmp_buf(debugger, current_fiber, result, arch)  # Optional: Add stack traces
    except Exception as err:
        print(f"Error: failed to get a stacktrace: {err}")

def print_fiber_registers(debugger, command, result, internal_dict, arch, fiber_id=None):
    """Prints the registers of a specified fiber."""
    target = debugger.GetSelectedTarget()
    process = target.GetProcess()

    if fiber_id is None:
        fibers_current_var = target.FindFirstGlobalVariable("fibers_current")
        if not fibers_current_var.IsValid():
            result.SetError("Could not find 'fibers_current' global variable.")
            return

        current_fiber = get_fiber_info(debugger, fibers_current_var)
        if not current_fiber:
            result.AppendMessage("No current fiber.")
            return

    else:
        # find the specified fiber in the existing queue
        fiber_address = None
        existing_queue_var = target.FindFirstGlobalVariable("fibers_existing_queue")
        if not existing_queue_var.IsValid():
            result.SetError("Could not find 'fibers_existing_queue' global variable.")
            return

        queue_top_value = existing_queue_var.GetChildMemberWithName('top')
        if not queue_top_value.IsValid():
            result.SetError(f"Could not find '{existing_queue_var.GetName()}.top' field.")
            return

        fiber_value = queue_top_value
        while fiber_value and fiber_value.IsValid():
            temp_fiber = get_fiber_info(debugger, fiber_value)
            if temp_fiber and temp_fiber['id'] == int(fiber_id):
                current_fiber = temp_fiber
                break

            next_fiber_value = fiber_value.GetChildMemberWithName('next_existing')
            if not next_fiber_value.IsValid():
                break

            fiber_value = next_fiber_value

        if not current_fiber:
            result.AppendMessage(f"Fiber with ID {fiber_id} not found.")
            return

    env_address = current_fiber["env_address"]
    error = lldb.SBError()
    addr_size = target.GetAddressByteSize()

    if arch == "x86_64":
        result.AppendMessage(f"  Error: Register printing is not supported on x86_64.")
        return

    elif arch == "arm64":
        # reference: libplatform src/setjmp/arm64/setjmp.s __longjmp
        X19_OFFSET = 0
        X20_OFFSET = 8
        X21_OFFSET = 16
        X22_OFFSET = 24
        X23_OFFSET = 32
        X24_OFFSET = 40
        X25_OFFSET = 48
        X26_OFFSET = 56
        X27_OFFSET = 64
        X28_OFFSET = 72

        FP_OFFSET = 80
        LR_OFFSET  = 88
        SP_OFFSET = 96
        
        x19 = process.ReadPointerFromMemory(env_address + X19_OFFSET, error)
        x20 = process.ReadPointerFromMemory(env_address + X20_OFFSET, error)
        x21 = process.ReadPointerFromMemory(env_address + X21_OFFSET, error)
        x22 = process.ReadPointerFromMemory(env_address + X22_OFFSET, error)
        x23 = process.ReadPointerFromMemory(env_address + X23_OFFSET, error)
        x24 = process.ReadPointerFromMemory(env_address + X24_OFFSET, error)
        x25 = process.ReadPointerFromMemory(env_address + X25_OFFSET, error)
        x26 = process.ReadPointerFromMemory(env_address + X26_OFFSET, error)
        x27 = process.ReadPointerFromMemory(env_address + X27_OFFSET, error)
        x28 = process.ReadPointerFromMemory(env_address + X28_OFFSET, error)

        fp  = process.ReadPointerFromMemory(env_address + FP_OFFSET, error)
        lr  = process.ReadPointerFromMemory(env_address + LR_OFFSET, error)
        sp  = process.ReadPointerFromMemory(env_address + SP_OFFSET, error)

        if error.Fail():
            result.AppendMessage(f"  Error: Could not read registers from jmp_buf: {error}")
            return
    
        strip_res = strip_fp_lr_sp(process, target, fp, lr, sp)
        if strip_res is None:
            result.AppendMessage(f"  Error: Could not strip FP LR or SP")
            return
        fp, lr, sp = strip_res

        result.AppendMessage(f"Fiber {current_fiber['id']} Registers (arm64):")
        result.AppendMessage("-----------------------------")
        result.AppendMessage(f"  X19: 0x{x19:x}")
        result.AppendMessage(f"  X20: 0x{x20:x}")
        result.AppendMessage(f"  X21: 0x{x21:x}")
        result.AppendMessage(f"  X22: 0x{x22:x}")
        result.AppendMessage(f"  X23: 0x{x23:x}")
        result.AppendMessage(f"  X24: 0x{x24:x}")
        result.AppendMessage(f"  X25: 0x{x25:x}")
        result.AppendMessage(f"  X26: 0x{x26:x}")
        result.AppendMessage(f"  X27: 0x{x27:x}")
        result.AppendMessage(f"  X28: 0x{x28:x}")
        result.AppendMessage(f"  LR:  0x{lr:x}")
        result.AppendMessage(f"  FP:  0x{fp:x}")
        result.AppendMessage(f"  SP:  0x{sp:x}")
    else:
        result.AppendMessage(f"  Error: Unsupported architecture: {arch}")
        return

arch = None

def list_fibers_all_cmd(debugger, command, result, internal_dict):
    list_fibers_all(debugger, command, result, internal_dict, arch)

def list_fibers_ready_cmd(debugger, command, result, internal_dict):
    list_fibers_ready(debugger, command, result, internal_dict, arch)

def get_current_fiber_info_cmd(debugger, command, result, internal_dict):
    get_current_fiber_info(debugger, command, result, internal_dict, arch)

def print_fiber_registers_cmd(debugger, command, result, internal_dict):
    """Prints the registers of a specified fiber."""
    args = command.split()
    fiber_id = None
    if len(args) > 0:
        try:
            fiber_id = int(args[0])
        except ValueError:
            result.SetError("Invalid fiber ID. Please provide an integer.")
            return

    print_fiber_registers(debugger, command, result, internal_dict, arch, fiber_id)

def __lldb_init_module(debugger, internal_dict):
    global arch
    """LLDB calls this function to load the script."""

    target = debugger.GetSelectedTarget()
    platform = target.GetPlatform()
    if platform:
        platform_name = platform.GetTriple()
        if "x86_64" in platform_name:
            arch = "x86_64"
        elif "arm64" in platform_name or "aarch64" in platform_name:
            arch = "arm64"
        else:
            print(f"Warning: Unsupported architecture: {platform_name}. Stack traces may not work.")
            arch = "unknown"
    else:
        print("Warning: Could not get platform information. Stack traces may not work.")
        arch = "unknown"

    debugger.HandleCommand('command script add -f fibers_lldb.list_fibers_all_cmd fibers_all')
    debugger.HandleCommand('command script add -f fibers_lldb.list_fibers_ready_cmd fibers_ready')
    debugger.HandleCommand('command script add -f fibers_lldb.get_current_fiber_info_cmd fibers_current')
    debugger.HandleCommand('command script add -f fibers_lldb.print_fiber_registers_cmd fibers_regs')
    print("The 'fibers_all', 'fibers_ready', 'fibers_current', and 'fibers_regs' commands have been added.")
    print(f"Detected architecture: {arch}")
