#include <sys/proc.h>
#include <sys/param.h>
#include <sys/user.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <pwd.h>

#include "ps.h"
#include <mach/shared_memory_server.h>
#include <mach/mach_vm.h>

extern kern_return_t task_read_for_pid(task_port_t task, pid_t pid, task_port_t *target);

#define STATE_MAX       7 
                
int     
mach_state_order(s, sleep_time)
int s;
long sleep_time;
{      
switch (s) {
case TH_STATE_RUNNING:      return(1);
case TH_STATE_UNINTERRUPTIBLE:
                                return(2);
    case TH_STATE_WAITING:      return((sleep_time > 20) ? 4 : 3);
    case TH_STATE_STOPPED:      return(5);
    case TH_STATE_HALTED:       return(6);  
    default:                    return(7); 
    }
}
                            /*01234567 */
char    mach_state_table[] = " RUSITH?";


int
thread_schedinfo(
	KINFO *ki,
	thread_port_t		thread,
	policy_t		pol,
	void * buf)
{
	unsigned int		count;
	int ret = KERN_FAILURE;

	switch (pol) {

	case POLICY_TIMESHARE:
		count = POLICY_TIMESHARE_INFO_COUNT;
		ret = thread_info(thread, THREAD_SCHED_TIMESHARE_INFO,
					(thread_info_t)buf, &count);
		if((ret == KERN_SUCCESS) && (ki->curpri < (((struct policy_timeshare_info *)buf)->cur_priority)))
			ki->curpri  = ((struct policy_timeshare_info *)buf)->cur_priority;
		break;

	case POLICY_FIFO:
		count = POLICY_FIFO_INFO_COUNT;
		ret = thread_info(thread, THREAD_SCHED_FIFO_INFO,
					buf, &count);
		if((ret == KERN_SUCCESS) && (ki->curpri < (((struct policy_fifo_info *)buf)->base_priority)))
			ki->curpri  = ((struct policy_fifo_info *)buf)->base_priority;
		break;

	case POLICY_RR:
		count = POLICY_RR_INFO_COUNT;
		ret = thread_info(thread, THREAD_SCHED_RR_INFO,
					buf, &count);
		if((ret == KERN_SUCCESS) && (ki->curpri < (((struct policy_rr_info *)buf)->base_priority)))
			ki->curpri  = ((struct policy_rr_info *)buf)->base_priority;
		break;
	}
	return(ret);
}

int get_task_info (KINFO *ki) 
{
	kern_return_t   	error;
	unsigned int		info_count = TASK_BASIC_INFO_COUNT;
        unsigned int 		thread_info_count = THREAD_BASIC_INFO_COUNT;
        pid_t				pid;
	int j, err = 0;

	ki->state = STATE_MAX;

	pid = KI_PROC(ki)->p_pid;
    error = task_read_for_pid(mach_task_self(), pid, &ki->task);
	if (error != KERN_SUCCESS) {
#ifdef DEBUG
        mach_error("Error calling task_read_for_pid()", error);
#endif
                return(1);
	}
	info_count = TASK_BASIC_INFO_COUNT;
	 error = task_info(ki->task, TASK_BASIC_INFO, (task_info_t)&ki->tasks_info, &info_count);
	 if (error != KERN_SUCCESS) {
		 ki->invalid_tinfo=1;
#ifdef DEBUG
		 mach_error("Error calling task_info()", error);
#endif
		return(1);
	}
	{
	        vm_region_basic_info_data_64_t    b_info;
		mach_vm_address_t	          address = GLOBAL_SHARED_TEXT_SEGMENT;
		mach_vm_size_t		          size;
		mach_port_t		          object_name;

		/*
		 * try to determine if this task has the split libraries
		 * mapped in... if so, adjust its virtual size down by
		 * the 2 segments that are used for split libraries
		 */
		info_count = VM_REGION_BASIC_INFO_COUNT_64;
		error = mach_vm_region(ki->task, &address, &size, VM_REGION_BASIC_INFO,
				     (vm_region_info_t)&b_info, &info_count, &object_name);
	        if (error == KERN_SUCCESS) {
		        if (b_info.reserved && size == (SHARED_TEXT_REGION_SIZE) &&
			    ki->tasks_info.virtual_size > (SHARED_TEXT_REGION_SIZE + SHARED_DATA_REGION_SIZE))
			        ki->tasks_info.virtual_size -= (SHARED_TEXT_REGION_SIZE + SHARED_DATA_REGION_SIZE);
		}
	}
	info_count = TASK_THREAD_TIMES_INFO_COUNT;
        error = task_info(ki->task, TASK_THREAD_TIMES_INFO, (task_info_t)&ki->times, &info_count);
        if (error != KERN_SUCCESS) {
                 ki->invalid_tinfo=1;
#ifdef DEBUG
                 mach_error("Error calling task_info()", error);
#endif
                return(1);
        }
	switch(ki->tasks_info.policy) {
		case POLICY_TIMESHARE :
		info_count = POLICY_TIMESHARE_INFO_COUNT;
		error = task_info(ki->task, TASK_SCHED_TIMESHARE_INFO, (task_info_t)&ki->schedinfo.tshare, &info_count);
			if (error != KERN_SUCCESS) {
				ki->invalid_tinfo=1;
#ifdef DEBUG
				mach_error("Error calling task_info()", error);
#endif
				return(1);
				}

			ki->curpri = ki->schedinfo.tshare.cur_priority;
			ki->basepri = ki->schedinfo.tshare.base_priority;
			break;
		case POLICY_RR :
 		info_count = POLICY_RR_INFO_COUNT;
		error = task_info(ki->task, TASK_SCHED_RR_INFO, (task_info_t)&ki->schedinfo.rr, &info_count);
			if (error != KERN_SUCCESS) {
				ki->invalid_tinfo=1;
#ifdef DEBUG
				mach_error("Error calling task_info()", error);
#endif
				return(1);
				}

                        ki->curpri = ki->schedinfo.rr.base_priority;
                        ki->basepri = ki->schedinfo.rr.base_priority;
                        break;

		case POLICY_FIFO :
  		info_count = POLICY_FIFO_INFO_COUNT;
		error = task_info(ki->task, TASK_SCHED_FIFO_INFO, (task_info_t)&ki->schedinfo.fifo, &info_count);
			if (error != KERN_SUCCESS) {
				ki->invalid_tinfo=1;
#ifdef DEBUG
				mach_error("Error calling task_info()", error);
#endif
				return(1);
				}

                        ki->curpri = ki->schedinfo.fifo.base_priority;
                        ki->basepri = ki->schedinfo.fifo.base_priority;
                        break;
	}

	 ki->invalid_tinfo=0;

	ki->cpu_usage=0;
	error = task_threads(ki->task, &ki->thread_list, &ki->thread_count);
	if (error != KERN_SUCCESS) {
		mach_port_deallocate(mach_task_self(),ki->task);
#ifdef DEBUG
		mach_error("Call to task_threads() failed", error);
#endif
		return(1);
	}
	err=0;
	//ki->curpri = 255;
	//ki->basepri = 255;
    ki->swapped = 1;
    ki->thval = calloc(ki->thread_count, sizeof(struct thread_values));
    for (j = 0; j < ki->thread_count; j++) {
        int tstate;
        thread_info_count = THREAD_BASIC_INFO_COUNT;
        error = thread_info(ki->thread_list[j], THREAD_BASIC_INFO,
                            (thread_info_t)&ki->thval[j].tb,
                            &thread_info_count);
        if (error != KERN_SUCCESS) {
#ifdef DEBUG
            mach_error("Call to thread_info() failed", error);
#endif
            err=1;
        } else {
            ki->cpu_usage += ki->thval[j].tb.cpu_usage;
        }
        error = thread_schedinfo(ki, ki->thread_list[j],
                                 ki->thval[j].tb.policy, &ki->thval[j].schedinfo);
        if (error != KERN_SUCCESS) {
#ifdef DEBUG
            mach_error("Call to thread_schedinfo() failed", error);
#endif
            err=1;
        }
        tstate = mach_state_order(ki->thval[j].tb.run_state,
                                  ki->thval[j].tb.sleep_time);
        if (tstate < ki->state)
            ki->state = tstate;
        if ((ki->thval[j].tb.flags & TH_FLAGS_SWAPPED ) == 0)
            ki->swapped = 0;
        mach_port_deallocate(mach_task_self(),
                             ki->thread_list[j]);
    }
    ki->invalid_thinfo = err;
	 /* Deallocate the list of threads. */
    error = vm_deallocate(mach_task_self(),
                          (vm_address_t)(ki->thread_list),
                          sizeof(*ki->thread_list) * ki->thread_count);
    if (error != KERN_SUCCESS) {
#ifdef DEBUG
        mach_error("Trouble freeing thread_list", error);
#endif
    }

    mach_port_deallocate(mach_task_self(),ki->task);
    return(0);
}
