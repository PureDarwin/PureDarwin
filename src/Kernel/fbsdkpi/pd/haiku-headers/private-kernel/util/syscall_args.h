// syscall_args.h

#ifndef _SYSCALL_ARGS_H
#define _SYSCALL_ARGS_H

#include <kernel.h>


// copy_ref_var_from_user
template<typename T>
inline
status_t
copy_ref_var_from_user(T *user, T &kernel)
{
	if (!IS_USER_ADDRESS(user))
		return B_BAD_ADDRESS;
	return user_memcpy(&kernel, user, sizeof(T));
}

// copy_ref_var_to_user
template<typename T>
inline
status_t
copy_ref_var_to_user(T &kernel, T *user)
{
	if (!IS_USER_ADDRESS(user))
		return B_BAD_ADDRESS;
	return user_memcpy(user, &kernel, sizeof(T));
}


#endif	// _SYSCALL_ARGS_H
