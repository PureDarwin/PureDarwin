// SPDX-License-Identifier: MIT
// entry points of display blocks left out of the build, dcn 4.01 (rdna4) and hdcp
// no supported asic reaches the dcn ones, hdcp calls report a failure status

void pd_amdgpu_absent_called(const char *name);
int printf(const char *fmt, ...);

void
pd_amdgpu_absent_called(const char *name)
{
	printf("amdgpu: %s is not built\n", name);
}

long
dal_hw_factory_dcn401_init(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dal_hw_translate_dcn401_init(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dcn401_clk_mgr_construct(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dcn401_clk_mgr_destroy(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dcn401_create_resource_pool(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dcn401_regs(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_backdoor_load(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_backdoor_load_zfb_mode(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_clear_inbox0_ack_register(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_configure_dmub_in_system_memory(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_enable_dmub_boot_options(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_enable_reg_inbox0_rsp_int(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_enable_reg_outbox0_rdy_int(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_get_current_time(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_get_diagnostic_data(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_get_fw_boot_status(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_get_gpint_dataout(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_get_gpint_response(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_get_inbox1_rptr(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_get_inbox1_wptr(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_get_outbox0_wptr(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_get_outbox1_wptr(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_is_gpint_acked(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_is_hw_init(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_is_supported(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_read_inbox0_ack_register(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_read_reg_inbox0_cmd_rsp(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_read_reg_inbox0_rsp_int_status(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_read_reg_outbox0_msg(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_read_reg_outbox0_rdy_int_status(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_read_reg_outbox0_rsp_int_status(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_reset(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_reset_release(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_send_inbox0_cmd(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_send_reg_inbox0_cmd_msg(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_set_gpint(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_set_inbox1_wptr(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_set_outbox0_rptr(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_set_outbox1_rptr(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_setup_mailbox(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_setup_outbox0(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_setup_out_mailbox(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_setup_windows(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_skip_dmub_panel_power_sequence(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_write_reg_inbox0_rsp_int_ack(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_write_reg_outbox0_rdy_int_ack(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dmub_dcn401_write_reg_outbox0_rsp(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
dscl401_spl_calc_lb_num_partitions(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
mod_hdcp_add_display(void)
{
	pd_amdgpu_absent_called(__func__);
	return 1;
}

long
mod_hdcp_process_event(void)
{
	pd_amdgpu_absent_called(__func__);
	return 1;
}

long
mod_hdcp_query_display(void)
{
	pd_amdgpu_absent_called(__func__);
	return 1;
}

long
mod_hdcp_remove_display(void)
{
	pd_amdgpu_absent_called(__func__);
	return 1;
}

long
mod_hdcp_reset_connection(void)
{
	pd_amdgpu_absent_called(__func__);
	return 1;
}

long
mod_hdcp_signal_type_to_operation_mode(void)
{
	pd_amdgpu_absent_called(__func__);
	return 1;
}

long
mod_hdcp_update_display(void)
{
	pd_amdgpu_absent_called(__func__);
	return 1;
}

long
mpc401_populate_lut(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

long
mpc401_set_movable_cm_location(void)
{
	pd_amdgpu_absent_called(__func__);
	return 0;
}

// register offsets dmub_srv takes the address of
const char dmub_srv_dcn401_regs[4096];
