/*
 * ASSERTION:
 *      Positive test to make sure that we can invoke x86
 *      vmregs[] aliases.
 *
 * SECTION: Virtual Machine Tracing/vmregs Array
 *
 * NOTES: This test does no verification - the value of the output
 *      is not deterministic.
 */

#pragma D option quiet

BEGIN
{
	printf("VMX_VIRTUAL_PROCESSOR_ID = 0x%x\n", vmregs[VMX_VIRTUAL_PROCESSOR_ID]);
	printf("VMX_GUEST_ES_SELECTOR = 0x%x\n", vmregs[VMX_GUEST_ES_SELECTOR]);
	printf("VMX_GUEST_CS_SELECTOR = 0x%x\n", vmregs[VMX_GUEST_CS_SELECTOR]);
	printf("VMX_GUEST_SS_SELECTOR = 0x%x\n", vmregs[VMX_GUEST_SS_SELECTOR]);
	printf("VMX_GUEST_DS_SELECTOR = 0x%x\n", vmregs[VMX_GUEST_DS_SELECTOR]);
	printf("VMX_GUEST_FS_SELECTOR = 0x%x\n", vmregs[VMX_GUEST_FS_SELECTOR]);
	printf("VMX_GUEST_GS_SELECTOR = 0x%x\n", vmregs[VMX_GUEST_GS_SELECTOR]);
	printf("VMX_GUEST_LDTR_SELECTOR = 0x%x\n", vmregs[VMX_GUEST_LDTR_SELECTOR]);
	printf("VMX_GUEST_TR_SELECTOR = 0x%x\n", vmregs[VMX_GUEST_TR_SELECTOR]);
	printf("VMX_HOST_ES_SELECTOR = 0x%x\n", vmregs[VMX_HOST_ES_SELECTOR]);
	printf("VMX_HOST_CS_SELECTOR = 0x%x\n", vmregs[VMX_HOST_CS_SELECTOR]);
	printf("VMX_HOST_SS_SELECTOR = 0x%x\n", vmregs[VMX_HOST_SS_SELECTOR]);
	printf("VMX_HOST_DS_SELECTOR = 0x%x\n", vmregs[VMX_HOST_DS_SELECTOR]);
	printf("VMX_HOST_FS_SELECTOR = 0x%x\n", vmregs[VMX_HOST_FS_SELECTOR]);
	printf("VMX_HOST_GS_SELECTOR = 0x%x\n", vmregs[VMX_HOST_GS_SELECTOR]);
	printf("VMX_HOST_TR_SELECTOR = 0x%x\n", vmregs[VMX_HOST_TR_SELECTOR]);
	printf("VMX_IO_BITMAP_A	 = 0x%x\n", vmregs[VMX_IO_BITMAP_A]);
	printf("VMX_IO_BITMAP_A_HIGH = 0x%x\n", vmregs[VMX_IO_BITMAP_A_HIGH]);
	printf("VMX_IO_BITMAP_B	 = 0x%x\n", vmregs[VMX_IO_BITMAP_B]);
	printf("VMX_IO_BITMAP_B_HIGH = 0x%x\n", vmregs[VMX_IO_BITMAP_B_HIGH]);
	printf("VMX_MSR_BITMAP = 0x%x\n", vmregs[VMX_MSR_BITMAP]);
	printf("VMX_MSR_BITMAP_HIGH = 0x%x\n", vmregs[VMX_MSR_BITMAP_HIGH]);
	printf("VMX_VM_EXIT_MSR_STORE_ADDR = 0x%x\n", vmregs[VMX_VM_EXIT_MSR_STORE_ADDR]);
	printf("VMX_VM_EXIT_MSR_STORE_ADDR_HIGH	 = 0x%x\n", vmregs[VMX_VM_EXIT_MSR_STORE_ADDR_HIGH]);
	printf("VMX_VM_EXIT_MSR_LOAD_ADDR = 0x%x\n", vmregs[VMX_VM_EXIT_MSR_LOAD_ADDR]);
	printf("VMX_VM_EXIT_MSR_LOAD_ADDR_HIGH = 0x%x\n", vmregs[VMX_VM_EXIT_MSR_LOAD_ADDR_HIGH ]);
	printf("VMX_VM_ENTRY_MSR_LOAD_ADDR = 0x%x\n", vmregs[VMX_VM_ENTRY_MSR_LOAD_ADDR]);
	printf("VMX_VM_ENTRY_MSR_LOAD_ADDR_HIGH	 = 0x%x\n", vmregs[VMX_VM_ENTRY_MSR_LOAD_ADDR_HIGH]);
	printf("VMX_TSC_OFFSET = 0x%x\n", vmregs[VMX_TSC_OFFSET]);
	printf("VMX_TSC_OFFSET_HIGH = 0x%x\n", vmregs[VMX_TSC_OFFSET_HIGH]);
	printf("VMX_VIRTUAL_APIC_PAGE_ADDR = 0x%x\n", vmregs[VMX_VIRTUAL_APIC_PAGE_ADDR]);
	printf("VMX_VIRTUAL_APIC_PAGE_ADDR_HIGH	 = 0x%x\n", vmregs[VMX_VIRTUAL_APIC_PAGE_ADDR_HIGH]);
	printf("VMX_APIC_ACCESS_ADDR = 0x%x\n", vmregs[VMX_APIC_ACCESS_ADDR]);
	printf("VMX_APIC_ACCESS_ADDR_HIGH = 0x%x\n", vmregs[VMX_APIC_ACCESS_ADDR_HIGH]);
	printf("VMX_EPT_POINTER	 = 0x%x\n", vmregs[VMX_EPT_POINTER]);
	printf("VMX_EPT_POINTER_HIGH = 0x%x\n", vmregs[VMX_EPT_POINTER_HIGH]);
	printf("VMX_GUEST_PHYSICAL_ADDRESS = 0x%x\n", vmregs[VMX_GUEST_PHYSICAL_ADDRESS]);
	printf("VMX_GUEST_PHYSICAL_ADDRESS_HIGH = 0x%x\n", vmregs[VMX_GUEST_PHYSICAL_ADDRESS_HIGH]);
	printf("VMX_VMCS_LINK_POINTER = 0x%x\n", vmregs[VMX_VMCS_LINK_POINTER]);
	printf("VMX_VMCS_LINK_POINTER_HIGH = 0x%x\n", vmregs[VMX_VMCS_LINK_POINTER_HIGH]);
	printf("VMX_GUEST_IA32_DEBUGCTL = 0x%x\n", vmregs[VMX_GUEST_IA32_DEBUGCTL]);
	printf("VMX_GUEST_IA32_DEBUGCTL_HIGH = 0x%x\n", vmregs[VMX_GUEST_IA32_DEBUGCTL_HIGH]);
	printf("VMX_GUEST_IA32_PAT = 0x%x\n", vmregs[VMX_GUEST_IA32_PAT]);
	printf("VMX_GUEST_IA32_PAT_HIGH	 = 0x%x\n", vmregs[VMX_GUEST_IA32_PAT_HIGH]);
	printf("VMX_GUEST_PDPTR0 = 0x%x\n", vmregs[VMX_GUEST_PDPTR0]);
	printf("VMX_GUEST_PDPTR0_HIGH = 0x%x\n", vmregs[VMX_GUEST_PDPTR0_HIGH]);
	printf("VMX_GUEST_PDPTR1 = 0x%x\n", vmregs[VMX_GUEST_PDPTR1]);
	printf("VMX_GUEST_PDPTR1_HIGH = 0x%x\n", vmregs[VMX_GUEST_PDPTR1_HIGH]);
	printf("VMX_GUEST_PDPTR2 = 0x%x\n", vmregs[VMX_GUEST_PDPTR2]);
	printf("VMX_GUEST_PDPTR2_HIGH = 0x%x\n", vmregs[VMX_GUEST_PDPTR2_HIGH]);
	printf("VMX_GUEST_PDPTR3 = 0x%x\n", vmregs[VMX_GUEST_PDPTR3]);
	printf("VMX_GUEST_PDPTR3_HIGH = 0x%x\n", vmregs[VMX_GUEST_PDPTR3_HIGH]);
	printf("VMX_HOST_IA32_PAT = 0x%x\n", vmregs[VMX_HOST_IA32_PAT]);
	printf("VMX_HOST_IA32_PAT_HIGH = 0x%x\n", vmregs[VMX_HOST_IA32_PAT_HIGH]);
	printf("VMX_PIN_BASED_VM_EXEC_CONTROL = 0x%x\n", vmregs[VMX_PIN_BASED_VM_EXEC_CONTROL]);
	printf("VMX_CPU_BASED_VM_EXEC_CONTROL = 0x%x\n", vmregs[VMX_CPU_BASED_VM_EXEC_CONTROL]);
	printf("VMX_EXCEPTION_BITMAP = 0x%x\n", vmregs[VMX_EXCEPTION_BITMAP]);
	printf("VMX_PAGE_FAULT_ERROR_CODE_MASK = 0x%x\n", vmregs[VMX_PAGE_FAULT_ERROR_CODE_MASK]);
	printf("VMX_PAGE_FAULT_ERROR_CODE_MATCH = 0x%x\n", vmregs[VMX_PAGE_FAULT_ERROR_CODE_MATCH]);
	printf("VMX_CR3_TARGET_COUNT = 0x%x\n", vmregs[VMX_CR3_TARGET_COUNT]);
	printf("VMX_VM_EXIT_CONTROLS = 0x%x\n", vmregs[VMX_VM_EXIT_CONTROLS]);
	printf("VMX_VM_EXIT_MSR_STORE_COUNT = 0x%x\n", vmregs[VMX_VM_EXIT_MSR_STORE_COUNT]);
	printf("VMX_VM_EXIT_MSR_LOAD_COUNT = 0x%x\n", vmregs[VMX_VM_EXIT_MSR_LOAD_COUNT]);
	printf("VMX_VM_ENTRY_CONTROLS = 0x%x\n", vmregs[VMX_VM_ENTRY_CONTROLS]);
	printf("VMX_VM_ENTRY_MSR_LOAD_COUNT = 0x%x\n", vmregs[VMX_VM_ENTRY_MSR_LOAD_COUNT]);
	printf("VMX_VM_ENTRY_INTR_INFO_FIELD = 0x%x\n", vmregs[VMX_VM_ENTRY_INTR_INFO_FIELD]);
	printf("VMX_VM_ENTRY_EXCEPTION_ERROR_CODE = 0x%x\n", vmregs[VMX_VM_ENTRY_EXCEPTION_ERROR_CODE]);
	printf("VMX_VM_ENTRY_INSTRUCTION_LEN = 0x%x\n", vmregs[VMX_VM_ENTRY_INSTRUCTION_LEN]);
	printf("VMX_TPR_THRESHOLD = 0x%x\n", vmregs[VMX_TPR_THRESHOLD]);
	printf("VMX_SECONDARY_VM_EXEC_CONTROL = 0x%x\n", vmregs[VMX_SECONDARY_VM_EXEC_CONTROL]);
	printf("VMX_PLE_GAP = 0x%x\n", vmregs[VMX_PLE_GAP]);
	printf("VMX_PLE_WINDOW = 0x%x\n", vmregs[VMX_PLE_WINDOW]);
	printf("VMX_VM_INSTRUCTION_ERROR = 0x%x\n", vmregs[VMX_VM_INSTRUCTION_ERROR]);
	printf("VMX_VM_EXIT_REASON = 0x%x\n", vmregs[VMX_VM_EXIT_REASON]);
	printf("VMX_VM_EXIT_INTR_INFO = 0x%x\n", vmregs[VMX_VM_EXIT_INTR_INFO]);
	printf("VMX_VM_EXIT_INTR_ERROR_CODE = 0x%x\n", vmregs[VMX_VM_EXIT_INTR_ERROR_CODE]);
	printf("VMX_IDT_VECTORING_INFO_FIELD = 0x%x\n", vmregs[VMX_IDT_VECTORING_INFO_FIELD]);
	printf("VMX_IDT_VECTORING_ERROR_CODE = 0x%x\n", vmregs[VMX_IDT_VECTORING_ERROR_CODE ]);
	printf("VMX_VM_EXIT_INSTRUCTION_LEN = 0x%x\n", vmregs[VMX_VM_EXIT_INSTRUCTION_LEN]);
	printf("VMX_VMX_INSTRUCTION_INFO = 0x%x\n", vmregs[VMX_VMX_INSTRUCTION_INFO]);
	printf("VMX_GUEST_ES_LIMIT = 0x%x\n", vmregs[VMX_GUEST_ES_LIMIT]);
	printf("VMX_GUEST_CS_LIMIT = 0x%x\n", vmregs[VMX_GUEST_CS_LIMIT]);
	printf("VMX_GUEST_SS_LIMIT = 0x%x\n", vmregs[VMX_GUEST_SS_LIMIT]);
	printf("VMX_GUEST_DS_LIMIT = 0x%x\n", vmregs[VMX_GUEST_DS_LIMIT]);
	printf("VMX_GUEST_FS_LIMIT = 0x%x\n", vmregs[VMX_GUEST_FS_LIMIT]);
	printf("VMX_GUEST_GS_LIMIT = 0x%x\n", vmregs[VMX_GUEST_GS_LIMIT]);
	printf("VMX_GUEST_LDTR_LIMIT = 0x%x\n", vmregs[VMX_GUEST_LDTR_LIMIT]);
	printf("VMX_GUEST_TR_LIMIT = 0x%x\n", vmregs[VMX_GUEST_TR_LIMIT]);
	printf("VMX_GUEST_GDTR_LIMIT = 0x%x\n", vmregs[VMX_GUEST_GDTR_LIMIT]);
	printf("VMX_GUEST_IDTR_LIMIT = 0x%x\n", vmregs[VMX_GUEST_IDTR_LIMIT]);
	printf("VMX_GUEST_ES_AR_BYTES = 0x%x\n", vmregs[VMX_GUEST_ES_AR_BYTES]);
	printf("VMX_GUEST_CS_AR_BYTES = 0x%x\n", vmregs[VMX_GUEST_CS_AR_BYTES]);
	printf("VMX_GUEST_SS_AR_BYTES = 0x%x\n", vmregs[VMX_GUEST_SS_AR_BYTES]);
	printf("VMX_GUEST_DS_AR_BYTES = 0x%x\n", vmregs[VMX_GUEST_DS_AR_BYTES]);
	printf("VMX_GUEST_FS_AR_BYTES = 0x%x\n", vmregs[VMX_GUEST_FS_AR_BYTES]);
	printf("VMX_GUEST_GS_AR_BYTES = 0x%x\n", vmregs[VMX_GUEST_GS_AR_BYTES]);
	printf("VMX_GUEST_LDTR_AR_BYTES = 0x%x\n", vmregs[VMX_GUEST_LDTR_AR_BYTES]);
	printf("VMX_GUEST_TR_AR_BYTES = 0x%x\n", vmregs[VMX_GUEST_TR_AR_BYTES]);
	printf("VMX_GUEST_INTERRUPTIBILITY_INFO = 0x%x\n", vmregs[VMX_GUEST_INTERRUPTIBILITY_INFO]);
	printf("VMX_GUEST_ACTIVITY_STATE = 0x%x\n", vmregs[VMX_GUEST_ACTIVITY_STATE]);
	printf("VMX_GUEST_SYSENTER_CS = 0x%x\n", vmregs[VMX_GUEST_SYSENTER_CS]);
	printf("VMX_HOST_IA32_SYSENTER_CS = 0x%x\n", vmregs[VMX_HOST_IA32_SYSENTER_CS]);
	printf("VMX_CR0_GUEST_HOST_MASK = 0x%x\n", vmregs[VMX_CR0_GUEST_HOST_MASK]);
	printf("VMX_CR4_GUEST_HOST_MASK = 0x%x\n", vmregs[VMX_CR4_GUEST_HOST_MASK]);
	printf("VMX_CR0_READ_SHADOW = 0x%x\n", vmregs[VMX_CR0_READ_SHADOW]);
	printf("VMX_CR4_READ_SHADOW = 0x%x\n", vmregs[VMX_CR4_READ_SHADOW]);
	printf("VMX_CR3_TARGET_VALUE0 = 0x%x\n", vmregs[VMX_CR3_TARGET_VALUE0]);
	printf("VMX_CR3_TARGET_VALUE1 = 0x%x\n", vmregs[VMX_CR3_TARGET_VALUE1]);
	printf("VMX_CR3_TARGET_VALUE2 = 0x%x\n", vmregs[VMX_CR3_TARGET_VALUE2]);
	printf("VMX_CR3_TARGET_VALUE3 = 0x%x\n", vmregs[VMX_CR3_TARGET_VALUE3]);
	printf("VMX_EXIT_QUALIFICATION = 0x%x\n", vmregs[VMX_EXIT_QUALIFICATION]);
	printf("VMX_GUEST_LINEAR_ADDRESS = 0x%x\n", vmregs[VMX_GUEST_LINEAR_ADDRESS]);
	printf("VMX_GUEST_CR0 = 0x%x\n", vmregs[VMX_GUEST_CR0]);
	printf("VMX_GUEST_CR3 = 0x%x\n", vmregs[VMX_GUEST_CR3]);
	printf("VMX_GUEST_CR4 = 0x%x\n", vmregs[VMX_GUEST_CR4]);
	printf("VMX_GUEST_ES_BASE = 0x%x\n", vmregs[VMX_GUEST_ES_BASE]);
	printf("VMX_GUEST_CS_BASE = 0x%x\n", vmregs[VMX_GUEST_CS_BASE]);
	printf("VMX_GUEST_SS_BASE = 0x%x\n", vmregs[VMX_GUEST_SS_BASE]);
	printf("VMX_GUEST_DS_BASE = 0x%x\n", vmregs[VMX_GUEST_DS_BASE]);
	printf("VMX_GUEST_FS_BASE = 0x%x\n", vmregs[VMX_GUEST_FS_BASE]);
	printf("VMX_GUEST_GS_BASE = 0x%x\n", vmregs[VMX_GUEST_GS_BASE]);
	printf("VMX_GUEST_LDTR_BASE = 0x%x\n", vmregs[VMX_GUEST_LDTR_BASE]);
	printf("VMX_GUEST_TR_BASE = 0x%x\n", vmregs[VMX_GUEST_TR_BASE]);
	printf("VMX_GUEST_GDTR_BASE = 0x%x\n", vmregs[VMX_GUEST_GDTR_BASE]);
	printf("VMX_GUEST_IDTR_BASE = 0x%x\n", vmregs[VMX_GUEST_IDTR_BASE]);
	printf("VMX_GUEST_DR7 = 0x%x\n", vmregs[VMX_GUEST_DR7]);
	printf("VMX_GUEST_RSP = 0x%x\n", vmregs[VMX_GUEST_RSP]);
	printf("VMX_GUEST_RIP = 0x%x\n", vmregs[VMX_GUEST_RIP]);
	printf("VMX_GUEST_RFLAGS = 0x%x\n", vmregs[VMX_GUEST_RFLAGS]);
	printf("VMX_GUEST_PENDING_DBG_EXCEPTIONS = 0x%x\n", vmregs[VMX_GUEST_PENDING_DBG_EXCEPTIONS]);
	printf("VMX_GUEST_SYSENTER_ESP = 0x%x\n", vmregs[VMX_GUEST_SYSENTER_ESP]);
	printf("VMX_GUEST_SYSENTER_EIP = 0x%x\n", vmregs[VMX_GUEST_SYSENTER_EIP]);
	printf("VMX_HOST_CR0 = 0x%x\n", vmregs[VMX_HOST_CR0]);
	printf("VMX_HOST_CR3 = 0x%x\n", vmregs[VMX_HOST_CR3]);
	printf("VMX_HOST_CR4 = 0x%x\n", vmregs[VMX_HOST_CR4]);
	printf("VMX_HOST_FS_BASE = 0x%x\n", vmregs[VMX_HOST_FS_BASE]);
	printf("VMX_HOST_GS_BASE = 0x%x\n", vmregs[VMX_HOST_GS_BASE]);
	printf("VMX_HOST_TR_BASE = 0x%x\n", vmregs[VMX_HOST_TR_BASE]);
	printf("VMX_HOST_GDTR_BASE = 0x%x\n", vmregs[VMX_HOST_GDTR_BASE]);
	printf("VMX_HOST_IDTR_BASE = 0x%x\n", vmregs[VMX_HOST_IDTR_BASE]);
	printf("VMX_HOST_IA32_SYSENTER_ESP = 0x%x\n", vmregs[VMX_HOST_IA32_SYSENTER_ESP]);
	printf("VMX_HOST_IA32_SYSENTER_EIP = 0x%x\n", vmregs[VMX_HOST_IA32_SYSENTER_EIP]);
	printf("VMX_HOST_RSP = 0x%x\n", vmregs[VMX_HOST_RSP]);
	printf("VMX_HOST_RIP = 0x%x\n", vmregs[VMX_HOST_RIP]);
	exit(0);
}

/*
 * Usage of vmregs[] causes ILLOP error when there is no virtual machine context running on the host.
 * This test covers the compiler only and the runtime error is ignored.
 */
ERROR
{
	exit(0);
}
