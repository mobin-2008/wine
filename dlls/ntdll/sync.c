/*
 *	Process synchronisation
 *
 * Copyright 1997 Alexandre Julliard
 * Copyright 1999, 2000 Juergen Schmied
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_SYS_POLL_H
# include <sys/poll.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NONAMELESSUNION
#define NONAMELESSSTRUCT

#include "winternl.h"
#include "async.h"
#include "thread.h"
#include "wine/server.h"
#include "wine/unicode.h"
#include "wine/debug.h"
#include "ntdll_misc.h"

WINE_DEFAULT_DEBUG_CHANNEL(ntdll);


/*
 *	Semaphores
 */

/******************************************************************************
 *  NtCreateSemaphore (NTDLL.@)
 */
NTSTATUS WINAPI NtCreateSemaphore( OUT PHANDLE SemaphoreHandle,
                                   IN ACCESS_MASK access,
                                   IN const OBJECT_ATTRIBUTES *attr OPTIONAL,
                                   IN ULONG InitialCount,
                                   IN ULONG MaximumCount )
{
    DWORD len = attr && attr->ObjectName ? attr->ObjectName->Length : 0;
    NTSTATUS ret;

    if ((MaximumCount <= 0) || (InitialCount > MaximumCount))
        return STATUS_INVALID_PARAMETER;

    SERVER_START_REQ( create_semaphore )
    {
        req->initial = InitialCount;
        req->max     = MaximumCount;
        req->inherit = attr && (attr->Attributes & OBJ_INHERIT);
        if (len) wine_server_add_data( req, attr->ObjectName->Buffer, len );
        ret = wine_server_call( req );
        *SemaphoreHandle = reply->handle;
    }
    SERVER_END_REQ;
    return ret;
}

/******************************************************************************
 *  NtOpenSemaphore (NTDLL.@)
 */
NTSTATUS WINAPI NtOpenSemaphore( OUT PHANDLE SemaphoreHandle,
                                 IN ACCESS_MASK access,
                                 IN const OBJECT_ATTRIBUTES *attr )
{
    DWORD len = attr && attr->ObjectName ? attr->ObjectName->Length : 0;
    NTSTATUS ret;

    SERVER_START_REQ( open_semaphore )
    {
        req->access  = access;
        req->inherit = attr && (attr->Attributes & OBJ_INHERIT);
        if (len) wine_server_add_data( req, attr->ObjectName->Buffer, len );
        ret = wine_server_call( req );
        *SemaphoreHandle = reply->handle;
    }
    SERVER_END_REQ;
    return ret;
}

/******************************************************************************
 *  NtQuerySemaphore (NTDLL.@)
 */
NTSTATUS WINAPI NtQuerySemaphore(
	HANDLE SemaphoreHandle,
	PVOID SemaphoreInformationClass,
	OUT PVOID SemaphoreInformation,
	ULONG Length,
	PULONG ReturnLength)
{
	FIXME("(%p,%p,%p,0x%08lx,%p) stub!\n",
	SemaphoreHandle, SemaphoreInformationClass, SemaphoreInformation, Length, ReturnLength);
	return STATUS_SUCCESS;
}

/******************************************************************************
 *  NtReleaseSemaphore (NTDLL.@)
 */
NTSTATUS WINAPI NtReleaseSemaphore( HANDLE handle, ULONG count, PULONG previous )
{
    NTSTATUS ret;
    SERVER_START_REQ( release_semaphore )
    {
        req->handle = handle;
        req->count  = count;
        if (!(ret = wine_server_call( req )))
        {
            if (previous) *previous = reply->prev_count;
        }
    }
    SERVER_END_REQ;
    return ret;
}

/*
 *	Events
 */

/**************************************************************************
 * NtCreateEvent (NTDLL.@)
 * ZwCreateEvent (NTDLL.@)
 */
NTSTATUS WINAPI NtCreateEvent(
	OUT PHANDLE EventHandle,
	IN ACCESS_MASK DesiredAccess,
	IN const OBJECT_ATTRIBUTES *attr,
	IN BOOLEAN ManualReset,
	IN BOOLEAN InitialState)
{
    DWORD len = attr && attr->ObjectName ? attr->ObjectName->Length : 0;
    NTSTATUS ret;

    SERVER_START_REQ( create_event )
    {
        req->manual_reset = ManualReset;
        req->initial_state = InitialState;
        req->inherit = attr && (attr->Attributes & OBJ_INHERIT);
        if (len) wine_server_add_data( req, attr->ObjectName->Buffer, len );
        ret = wine_server_call( req );
        *EventHandle = reply->handle;
    }
    SERVER_END_REQ;
    return ret;
}

/******************************************************************************
 *  NtOpenEvent (NTDLL.@)
 *  ZwOpenEvent (NTDLL.@)
 */
NTSTATUS WINAPI NtOpenEvent(
	OUT PHANDLE EventHandle,
	IN ACCESS_MASK DesiredAccess,
	IN const OBJECT_ATTRIBUTES *attr )
{
    DWORD len = attr && attr->ObjectName ? attr->ObjectName->Length : 0;
    NTSTATUS ret;

    SERVER_START_REQ( open_event )
    {
        req->access  = DesiredAccess;
        req->inherit = attr && (attr->Attributes & OBJ_INHERIT);
        if (len) wine_server_add_data( req, attr->ObjectName->Buffer, len );
        ret = wine_server_call( req );
        *EventHandle = reply->handle;
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *  NtSetEvent (NTDLL.@)
 *  ZwSetEvent (NTDLL.@)
 */
NTSTATUS WINAPI NtSetEvent( HANDLE handle, PULONG NumberOfThreadsReleased )
{
    NTSTATUS ret;

    /* FIXME: set NumberOfThreadsReleased */

    SERVER_START_REQ( event_op )
    {
        req->handle = handle;
        req->op     = SET_EVENT;
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;
    return ret;
}

/******************************************************************************
 *  NtResetEvent (NTDLL.@)
 */
NTSTATUS WINAPI NtResetEvent( HANDLE handle, PULONG NumberOfThreadsReleased )
{
    NTSTATUS ret;

    /* resetting an event can't release any thread... */
    if (NumberOfThreadsReleased) *NumberOfThreadsReleased = 0;

    SERVER_START_REQ( event_op )
    {
        req->handle = handle;
        req->op     = RESET_EVENT;
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;
    return ret;
}

/******************************************************************************
 *  NtClearEvent (NTDLL.@)
 *
 * FIXME
 *   same as NtResetEvent ???
 */
NTSTATUS WINAPI NtClearEvent ( HANDLE handle )
{
    return NtResetEvent( handle, NULL );
}

/******************************************************************************
 *  NtPulseEvent (NTDLL.@)
 *
 * FIXME
 *   PulseCount
 */
NTSTATUS WINAPI NtPulseEvent( HANDLE handle, PULONG PulseCount )
{
    NTSTATUS ret;
    FIXME("(%p,%p)\n", handle, PulseCount);
    SERVER_START_REQ( event_op )
    {
        req->handle = handle;
        req->op     = PULSE_EVENT;
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;
    return ret;
}

/******************************************************************************
 *  NtQueryEvent (NTDLL.@)
 */
NTSTATUS WINAPI NtQueryEvent (
	IN  HANDLE EventHandle,
	IN  UINT EventInformationClass,
	OUT PVOID EventInformation,
	IN  ULONG EventInformationLength,
	OUT PULONG  ReturnLength)
{
	FIXME("(%p)\n", EventHandle);
	return STATUS_SUCCESS;
}


/***********************************************************************
 *           check_async_list
 *
 * Process a status event from the server.
 */
static void WINAPI check_async_list(async_private *asp, DWORD status)
{
    async_private *ovp;
    DWORD ovp_status;

    for( ovp = NtCurrentTeb()->pending_list; ovp && ovp != asp; ovp = ovp->next );

    if(!ovp)
            return;

    if( status != STATUS_ALERTED )
    {
        ovp_status = status;
        ovp->ops->set_status (ovp, status);
    }
    else ovp_status = ovp->ops->get_status (ovp);

    if( ovp_status == STATUS_PENDING ) ovp->func( ovp );

    /* This will destroy all but PENDING requests */
    register_old_async( ovp );
}


/***********************************************************************
 *              wait_reply
 *
 * Wait for a reply on the waiting pipe of the current thread.
 */
static int wait_reply( void *cookie )
{
    int signaled;
    struct wake_up_reply reply;
    for (;;)
    {
        int ret;
        ret = read( NtCurrentTeb()->wait_fd[0], &reply, sizeof(reply) );
        if (ret == sizeof(reply))
        {
            if (!reply.cookie) break;  /* thread got killed */
            if (reply.cookie == cookie) return reply.signaled;
            /* we stole another reply, wait for the real one */
            signaled = wait_reply( cookie );
            /* and now put the wrong one back in the pipe */
            for (;;)
            {
                ret = write( NtCurrentTeb()->wait_fd[1], &reply, sizeof(reply) );
                if (ret == sizeof(reply)) break;
                if (ret >= 0) server_protocol_error( "partial wakeup write %d\n", ret );
                if (errno == EINTR) continue;
                server_protocol_perror("wakeup write");
            }
            return signaled;
        }
        if (ret >= 0) server_protocol_error( "partial wakeup read %d\n", ret );
        if (errno == EINTR) continue;
        server_protocol_perror("wakeup read");
    }
    /* the server closed the connection; time to die... */
    SYSDEPS_AbortThread(0);
}


/***********************************************************************
 *              call_apcs
 *
 * Call outstanding APCs.
 */
static void call_apcs( BOOL alertable )
{
    FARPROC proc = NULL;
    LARGE_INTEGER time;
    void *args[4];

    for (;;)
    {
        int type = APC_NONE;
        SERVER_START_REQ( get_apc )
        {
            req->alertable = alertable;
            wine_server_set_reply( req, args, sizeof(args) );
            if (!wine_server_call( req ))
            {
                type = reply->type;
                proc = reply->func;
            }
        }
        SERVER_END_REQ;

        switch(type)
        {
        case APC_NONE:
            return;  /* no more APCs */
        case APC_ASYNC:
            proc( args[0], args[1]);
            break;
        case APC_USER:
            proc( args[0] );
            break;
        case APC_TIMER:
            /* convert sec/usec to NT time */
            RtlSecondsSince1970ToTime( (time_t)args[0], &time );
            time.QuadPart += (DWORD)args[1] * 10;
            proc( args[2], time.s.LowPart, time.s.HighPart );
            break;
        case APC_ASYNC_IO:
            check_async_list ( args[0], (DWORD) args[1]);
            break;
        default:
            server_protocol_error( "get_apc_request: bad type %d\n", type );
            break;
        }
    }
}

/* wait operations */

/******************************************************************
 *		NtWaitForMultipleObjects (NTDLL.@)
 */
NTSTATUS WINAPI NtWaitForMultipleObjects( DWORD count, const HANDLE *handles,
                                          BOOLEAN wait_all, BOOLEAN alertable,
                                          PLARGE_INTEGER timeout )
{
    int ret, cookie;
    struct timeval tv;

    if (count > MAXIMUM_WAIT_OBJECTS) return STATUS_INVALID_PARAMETER_1;

    NTDLL_get_server_timeout( &tv, timeout );

    for (;;)
    {
        SERVER_START_REQ( select )
        {
            req->flags   = SELECT_INTERRUPTIBLE;
            req->cookie  = &cookie;
            req->sec     = tv.tv_sec;
            req->usec    = tv.tv_usec;
            wine_server_add_data( req, handles, count * sizeof(HANDLE) );

            if (wait_all) req->flags |= SELECT_ALL;
            if (alertable) req->flags |= SELECT_ALERTABLE;
            if (timeout) req->flags |= SELECT_TIMEOUT;

            ret = wine_server_call( req );
        }
        SERVER_END_REQ;
        if (ret == STATUS_PENDING) ret = wait_reply( &cookie );
        if (ret != STATUS_USER_APC) break;
        call_apcs( alertable );
        if (alertable) break;
    }
    return ret;
}


/******************************************************************
 *		NtWaitForSingleObject (NTDLL.@)
 */
NTSTATUS WINAPI NtWaitForSingleObject(HANDLE handle, BOOLEAN alertable, PLARGE_INTEGER timeout )
{
    return NtWaitForMultipleObjects( 1, &handle, FALSE, alertable, timeout );
}
