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

#include "DAThread.h"

#include <pthread.h>
#include <sysexits.h>
#include <mach/mach.h>

enum
{
    __kDAThreadRunLoopSourceJobKindExecute = 0x00000001
};

typedef UInt32 __DAThreadRunLoopSourceJobKind; 

struct __DAThreadRunLoopSourceJob
{
    __DAThreadRunLoopSourceJobKind      kind;
    struct __DAThreadRunLoopSourceJob * next;

    union
    {
        struct
        {
            Boolean                 exited;
            int                     status;
            pthread_t               thread;
            DAThreadExecuteCallback callback;
            void *                  callbackContext;
            DAThreadFunction        function;
            void *                  functionContext;
        } execute;
    };
};

typedef struct __DAThreadRunLoopSourceJob __DAThreadRunLoopSourceJob;

static __DAThreadRunLoopSourceJob * __gDAThreadRunLoopSourceJobs = NULL;
static pthread_mutex_t              __gDAThreadRunLoopSourceLock = PTHREAD_MUTEX_INITIALIZER;
static CFMachPortRef                __gDAThreadRunLoopSourcePort = NULL;

static void * __DAThreadFunction( void * context )
{
    /*
     * Run a thread.
     */

    __DAThreadRunLoopSourceJob * job;

    pthread_mutex_lock( &__gDAThreadRunLoopSourceLock );

    for ( job = __gDAThreadRunLoopSourceJobs; job; job = job->next )
    {
        assert( job->kind == __kDAThreadRunLoopSourceJobKindExecute );

        if ( pthread_equal( job->execute.thread, pthread_self( ) ) )
        {
            break;
        }
    }

    pthread_mutex_unlock( &__gDAThreadRunLoopSourceLock );

    if ( job )
    {
        mach_msg_header_t message;
        kern_return_t     status;

        job->execute.status = ( ( DAThreadFunction ) job->execute.function )( job->execute.functionContext );

        pthread_mutex_lock( &__gDAThreadRunLoopSourceLock );

        job->execute.exited = TRUE;

        pthread_mutex_unlock( &__gDAThreadRunLoopSourceLock );

        message.msgh_bits        = MACH_MSGH_BITS( MACH_MSG_TYPE_COPY_SEND, 0 );
        message.msgh_id          = 0;
        message.msgh_local_port  = MACH_PORT_NULL;
        message.msgh_remote_port = CFMachPortGetPort( __gDAThreadRunLoopSourcePort );
        message.msgh_reserved    = 0;
        message.msgh_size        = sizeof( message );

        status = mach_msg( &message, MACH_SEND_MSG | MACH_SEND_TIMEOUT, message.msgh_size, 0, MACH_PORT_NULL, 0, MACH_PORT_NULL );

        if ( status == MACH_SEND_TIMED_OUT )
        {
            mach_msg_destroy( &message );
        }
    }

    pthread_detach( pthread_self( ) );

    return NULL;
}

static void __DAThreadRunLoopSourceCallback( CFMachPortRef port, void * message, CFIndex messageSize, void * info )
{
    /*
     * Process a DAThread CFRunLoopSource fire.
     */

    __DAThreadRunLoopSourceJob * job     = NULL;
    __DAThreadRunLoopSourceJob * jobLast = NULL;

    pthread_mutex_lock( &__gDAThreadRunLoopSourceLock );

    /*
     * Scan through job list.
     */

    for ( job = __gDAThreadRunLoopSourceJobs; job; jobLast = NULL )
    {
        for ( job = __gDAThreadRunLoopSourceJobs; job; jobLast = job, job = job->next )
        {
            assert( job->kind == __kDAThreadRunLoopSourceJobKindExecute );

            if ( job->execute.exited )
            {
                /*
                 * Process the job's callback.
                 */

                if ( jobLast )
                {
                    jobLast->next = job->next;
                }
                else
                {
                    __gDAThreadRunLoopSourceJobs = job->next;
                }

                pthread_mutex_unlock( &__gDAThreadRunLoopSourceLock );

                /*
                 * Issue the callback.
                 */

                if ( job->execute.callback )
                {
                    ( job->execute.callback )( job->execute.status, job->execute.callbackContext );
                }

                /*
                 * Release our resources.
                 */

                free( job );

                pthread_mutex_lock( &__gDAThreadRunLoopSourceLock );

                break;
            }
        }
    }

    pthread_mutex_unlock( &__gDAThreadRunLoopSourceLock );
}

CFRunLoopSourceRef DAThreadCreateRunLoopSource( CFAllocatorRef allocator, CFIndex order )
{
    /*
     * Create a CFRunLoopSource for DAThread callbacks.
     */

    CFRunLoopSourceRef source = NULL;

    /*
     * Initialize our minimal state.
     */

    if ( __gDAThreadRunLoopSourcePort == NULL )
    {
        /*
         * Create the global CFMachPort.  It will be used to post jobs to the run loop.
         */

        __gDAThreadRunLoopSourcePort = CFMachPortCreate( kCFAllocatorDefault, __DAThreadRunLoopSourceCallback, NULL, NULL );

        if ( __gDAThreadRunLoopSourcePort )
        {
            /*
             * Set up the global CFMachPort.  It requires no more than one queue element.
             */

            mach_port_limits_t limits = { 0 };

            limits.mpl_qlimit = 1;

            mach_port_set_attributes( mach_task_self( ),
                                      CFMachPortGetPort( __gDAThreadRunLoopSourcePort ),
                                      MACH_PORT_LIMITS_INFO,
                                      ( mach_port_info_t ) &limits,
                                      MACH_PORT_LIMITS_INFO_COUNT );
        }
    }

    /*
     * Obtain the CFRunLoopSource for our CFMachPort.
     */

    if ( __gDAThreadRunLoopSourcePort )
    {
        source = CFMachPortCreateRunLoopSource( allocator, __gDAThreadRunLoopSourcePort, order );
    }

    return source;
}

void DAThreadExecute( DAThreadFunction function, void * functionContext, DAThreadExecuteCallback callback, void * callbackContext )
{
    /*
     * Execute a thread.
     */

    pthread_t thread;
    int       status;

    /*
     * State our assumptions.
     */

    assert( __gDAThreadRunLoopSourcePort );

    /*
     * Run the thread.
     */

    pthread_mutex_lock( &__gDAThreadRunLoopSourceLock );

    status = pthread_create( &thread, NULL, __DAThreadFunction, NULL );

    if ( status == 0 )
    {
        /*
         * Register this callback job on our queue.
         */

        __DAThreadRunLoopSourceJob * job;

        job = malloc( sizeof( __DAThreadRunLoopSourceJob ) );

        if ( job )
        {
            job->kind = __kDAThreadRunLoopSourceJobKindExecute;
            job->next = __gDAThreadRunLoopSourceJobs;

            job->execute.exited          = FALSE;
            job->execute.status          = 0;
            job->execute.thread          = thread;
            job->execute.callback        = callback;
            job->execute.callbackContext = callbackContext;
            job->execute.function        = function;
            job->execute.functionContext = functionContext;

            __gDAThreadRunLoopSourceJobs = job;
        }
    }

    pthread_mutex_unlock( &__gDAThreadRunLoopSourceLock );

    /*
     * Complete the call in case we had a local failure.
     */

    if ( status )
    {
        if ( callback )
        {
            ( callback )( EX_OSERR, callbackContext );
        }
    }
}
