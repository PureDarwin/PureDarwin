/*
 * Copyright (c) 1998-2014 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "DACommand.h"

#include "DABase.h"
#include "DAInternal.h"

#include <fcntl.h>
#include <paths.h>
#include <pthread.h>
#include <sysexits.h>
#include <unistd.h>
#include <mach/mach.h>
#include <sys/wait.h>
#include <spawn.h>
#include <crt_externs.h>

enum
{
    __kDACommandRunLoopSourceJobKindExecute = 0x00000001
};

typedef UInt32 __DACommandRunLoopSourceJobKind; 

struct __DACommandRunLoopSourceJob
{
    __DACommandRunLoopSourceJobKind      kind;
    struct __DACommandRunLoopSourceJob * next;

    union
    {
        struct
        {
            pid_t                    pid;
            int                      pipe;
            DACommandExecuteCallback callback;
            void *                   callbackContext;
        } execute;
    };
};

typedef struct __DACommandRunLoopSourceJob __DACommandRunLoopSourceJob;

static __DACommandRunLoopSourceJob * __gDACommandRunLoopSourceJobs = NULL;
static pthread_mutex_t               __gDACommandRunLoopSourceLock = PTHREAD_MUTEX_INITIALIZER;
static CFMachPortRef                 __gDACommandRunLoopSourcePort = NULL;

static void __DACommandExecute( char * const *           argv,
                                UInt32                   options,
                                uid_t                    userUID,
                                gid_t                    userGID,
                                DACommandExecuteCallback callback,
                                void *                   callbackContext )
{
    /*
     * Execute a command as the specified user.  The argument list must be NULL terminated.
     */

    pid_t           executablePID = 0;
    int             outputPipe[2] = { -1, -1 };
    int             status        = EX_OK;

    /*
     * State our assumptions.
     */

    assert( __gDACommandRunLoopSourcePort );

    /*
     * Create a pipe in order to capture the executable output.
     */

    if ( ( options & kDACommandExecuteOptionCaptureOutput ) )
    {
        status = pipe( outputPipe );
        if ( status )  { status = EX_NOINPUT; goto __DACommandExecuteErr; }
    }

    /*
     * Fork in order to run the executable.
     */

    pthread_mutex_lock( &__gDACommandRunLoopSourceLock );

    executablePID = fork( );

    if ( executablePID == 0 )
    {
        int fd;
        int rt_val;
        posix_spawnattr_t attr, *attrp;
        posix_spawn_file_actions_t file_actions, *file_actionsp;;

        attrp = NULL;
        file_actionsp = NULL;

        /*
         * Prepare the post-fork execution environment.
         */

        rt_val = setgid( userGID );
        if (rt_val != -1) {
            rt_val = setuid( userUID );
        }

        if (rt_val == -1) {
            _exit( EX_NOPERM );
        }

        if ( outputPipe[1] != -1 )
        {
            dup2( outputPipe[1], STDOUT_FILENO );

            close( outputPipe[1] );
        }

        status = posix_spawnattr_init(&attr);
        if ( status )  { goto spawn_destroy; }
        attrp = &attr;
        status = posix_spawn_file_actions_init(&file_actions);
        if ( status )  { goto spawn_destroy; }
        file_actionsp = &file_actions;
        status = posix_spawnattr_setflags(&attr, POSIX_SPAWN_CLOEXEC_DEFAULT | POSIX_SPAWN_SETEXEC);
        if ( status )  { goto spawn_destroy; }
        status = posix_spawn_file_actions_addinherit_np(&file_actions, STDOUT_FILENO);
        if ( status )  { goto spawn_destroy; }
        status = posix_spawn_file_actions_addinherit_np(&file_actions, STDERR_FILENO);
        if ( status )  { goto spawn_destroy; }
        status = posix_spawn_file_actions_addinherit_np(&file_actions, STDIN_FILENO);
        if ( status )  { goto spawn_destroy; }

        /*
         * Run the executable.
         */
        posix_spawn(NULL, argv[0], &file_actions, &attr, argv, *_NSGetEnviron());

spawn_destroy:

        if (file_actionsp != NULL) {
            posix_spawn_file_actions_destroy(file_actionsp);
        }

        if (attrp != NULL) {
            posix_spawnattr_destroy(attrp);
        }
        _exit( EX_OSERR );
    }

    if ( executablePID != -1 )
    {
        /*
         * Register this callback job on our queue.
         */

        if ( callback )
        {
            __DACommandRunLoopSourceJob * job;

            job = malloc( sizeof( __DACommandRunLoopSourceJob ) );

            if ( job )
            {
                job->kind = __kDACommandRunLoopSourceJobKindExecute;
                job->next = __gDACommandRunLoopSourceJobs;

                job->execute.pid             = executablePID;
                job->execute.pipe            = ( outputPipe[0] != -1 ) ? dup( outputPipe[0] ) : -1;
                job->execute.callback        = callback;
                job->execute.callbackContext = callbackContext;

                __gDACommandRunLoopSourceJobs = job;
            }
        }
    }

    pthread_mutex_unlock( &__gDACommandRunLoopSourceLock );

    if ( executablePID == -1 )  { status = EX_OSERR; goto __DACommandExecuteErr; }

    /*
     * Release our resources.
     */

__DACommandExecuteErr:

    if ( outputPipe[0] != -1 )  close( outputPipe[0] );
    if ( outputPipe[1] != -1 )  close( outputPipe[1] );

    /*
     * Complete the call in case we had a local failure.
     */

    if ( status )
    {
        if ( callback )
        {
            ( callback )( status, NULL, callbackContext );
        }
    }
}

static void __DACommandRunLoopSourceCallback( CFMachPortRef port, void * message, CFIndex messageSize, void * info )
{
    /*
     * Process a DACommand CFRunLoopSource fire.  __DACommandSignal() triggers the fire when
     * a child exits or stops.  We locate the appropriate callback candidate in our job list
     * and issue the callback.
     */

    pid_t pid;
    int   status;

    /*
     * Scan through exited or stopped children.
     */

    while ( ( pid = waitpid( -1, &status, WNOHANG ) ) > 0 )
    {
        __DACommandRunLoopSourceJob * job     = NULL;
        __DACommandRunLoopSourceJob * jobLast = NULL;

        pthread_mutex_lock( &__gDACommandRunLoopSourceLock );

        /*
         * Scan through job list.
         */

        for ( job = __gDACommandRunLoopSourceJobs; job; jobLast = job, job = job->next )
        {
            assert( job->kind == __kDACommandRunLoopSourceJobKindExecute );

            if ( job->execute.pid == pid )
            {
                /*
                 * Process the job's callback.
                 */

                CFMutableDataRef output = NULL;

                if ( jobLast )
                {
                    jobLast->next = job->next;
                }
                else
                {
                    __gDACommandRunLoopSourceJobs = job->next;
                }

                pthread_mutex_unlock( &__gDACommandRunLoopSourceLock );

                /*
                 * Capture the executable's output, or the last remains of it, from the pipe.
                 */

                if ( job->execute.pipe != -1 )
                {
                    output = CFDataCreateMutable( kCFAllocatorDefault, 0 );

                    if ( output )
                    {
                        UInt8 * buffer;
                        
                        buffer = malloc( PIPE_BUF );

                        if ( buffer )
                        {
                            int count;

                            while ( ( count = read( job->execute.pipe, buffer, PIPE_BUF ) ) > 0 )
                            {
                                CFDataAppendBytes( output, buffer, count );
                            }

                            free( buffer );
                        }
                    }

                    close( job->execute.pipe );
                }

                /*
                 * Issue the callback.
                 */

                status = WIFEXITED( status ) ? ( ( char ) WEXITSTATUS( status ) ) : status;

                ( job->execute.callback )( status, output, job->execute.callbackContext );

                /*
                 * Release our resources.
                 */

                if ( output )
                {
                    CFRelease( output );
                }

                free( job );

                pthread_mutex_lock( &__gDACommandRunLoopSourceLock );

                break;
            }
        }

        pthread_mutex_unlock( &__gDACommandRunLoopSourceLock );
    }
}

static void __DACommandSignal( int sig )
{
    /*
     * Process a SIGCHLD signal.  mach_msg() is safe from a signal handler.
     */

    mach_msg_header_t message;
    kern_return_t     status;

    message.msgh_bits        = MACH_MSGH_BITS( MACH_MSG_TYPE_COPY_SEND, 0 );
    message.msgh_id          = 0;
    message.msgh_local_port  = MACH_PORT_NULL;
    message.msgh_remote_port = CFMachPortGetPort( __gDACommandRunLoopSourcePort );
    message.msgh_reserved    = 0;
    message.msgh_size        = sizeof( message );

    status = mach_msg( &message, MACH_SEND_MSG | MACH_SEND_TIMEOUT, message.msgh_size, 0, MACH_PORT_NULL, 0, MACH_PORT_NULL );

    if ( status == MACH_SEND_TIMED_OUT )
    {
        mach_msg_destroy( &message );
    }
}

CFRunLoopSourceRef DACommandCreateRunLoopSource( CFAllocatorRef allocator, CFIndex order )
{
    /*
     * Create a CFRunLoopSource for DACommand callbacks.
     */

    CFRunLoopSourceRef source = NULL;

    pthread_mutex_lock( &__gDACommandRunLoopSourceLock );

    /*
     * Initialize our minimal state.
     */

    if ( __gDACommandRunLoopSourcePort == NULL )
    {
        /*
         * Create the global CFMachPort.  It will be used to post jobs to the run loop.
         */

        __gDACommandRunLoopSourcePort = CFMachPortCreate( kCFAllocatorDefault, __DACommandRunLoopSourceCallback, NULL, NULL );

        if ( __gDACommandRunLoopSourcePort )
        {
            /*
             * Set up the global CFMachPort.  It requires no more than one queue element.
             */

            mach_port_limits_t limits = { 0 };

            limits.mpl_qlimit = 1;

            mach_port_set_attributes( mach_task_self( ),
                                      CFMachPortGetPort( __gDACommandRunLoopSourcePort ),
                                      MACH_PORT_LIMITS_INFO,
                                      ( mach_port_info_t ) &limits,
                                      MACH_PORT_LIMITS_INFO_COUNT );
        }

        if ( __gDACommandRunLoopSourcePort )
        {
            /*
             * Set up the global signal handler to catch child status changes from BSD.
             */

            sig_t sig;

            sig = signal( SIGCHLD, __DACommandSignal );

            if ( sig == SIG_ERR )
            {
                CFRelease( __gDACommandRunLoopSourcePort );

                __gDACommandRunLoopSourcePort = NULL;
            }
        }
    }

    /*
     * Obtain the CFRunLoopSource for our CFMachPort.
     */

    if ( __gDACommandRunLoopSourcePort )
    {
        source = CFMachPortCreateRunLoopSource( allocator, __gDACommandRunLoopSourcePort, order );
    }

    pthread_mutex_unlock( &__gDACommandRunLoopSourceLock );

    return source;
}

void DACommandExecute( CFURLRef                 executable,
                       DACommandExecuteOptions  options,
                       uid_t                    userUID,
                       gid_t                    userGID,
                       DACommandExecuteCallback callback,
                       void *                   callbackContext,
                       ... )
{
    /*
     * Execute a command as the specified user.  The argument list maps to argv[1] and up.  All
     * arguments in the argument list shall be of type CFTypeRef, which are converted to string
     * form via CFCopyDescription().  The argument list must be NULL terminated.
     */

    int         argc      = 0;
    char **     argv      = NULL;
    CFTypeRef   argument  = NULL;
    va_list     arguments;
    int         status    = EX_OK;

    /*
     * Construct the list of arguments -- compute argc.
     */

    va_start( arguments, callbackContext );

    for ( argc = 1; va_arg( arguments, CFTypeRef ); argc++ )  {  }

    va_end( arguments );

    /*
     * Construct the list of arguments -- allocate argv.
     */

    argv = malloc( ( argc + 1 ) * sizeof( char * ) );
    if ( argv == NULL )  { status = EX_SOFTWARE; goto DACommandExecuteErr; }

    memset( argv, 0, ( argc + 1 ) * sizeof( char * ) );

    /*
     * Construct the list of arguments -- fill out argv[0].
     */

    argv[0] = ___CFURLCopyFileSystemRepresentation( executable );
    if ( argv[0] == NULL )  { status = EX_DATAERR; goto DACommandExecuteErr; }

    /*
     * Construct the list of arguments -- fill out argv[1] through argv[argc].
     */

    va_start( arguments, callbackContext );

    for ( argc = 1; ( argument = va_arg( arguments, CFTypeRef ) ); argc++ )
    {
        CFStringRef string;

        string = CFStringCreateWithFormat( kCFAllocatorDefault, 0, CFSTR( "%@" ), argument );

        if ( string )
        {
            argv[argc] = ___CFStringCopyCString( string );

            CFRelease( string );
        }

        if ( argv[argc] == NULL )  break;
    }

    va_end( arguments );

    if ( argument )  { status = EX_SOFTWARE; goto DACommandExecuteErr; }

    /*
     * Run the executable.
     */

    __DACommandExecute( argv, options, userUID, userGID, callback, callbackContext );

    /*
     * Release our resources.
     */

DACommandExecuteErr:

    if ( argv )
    {
        for ( argc = 0; argv[argc]; argc++ )
        {
            free( argv[argc] );
        }

        free( argv );
    }

    /*
     * Complete the call in case we had a local failure.
     */

    if ( status )
    {
        if ( callback )
        {
            ( callback )( status, NULL, callbackContext );
        }
    }
}
