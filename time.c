/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993, 1994, 1995, 1996 Rick Sladkey <jrs@world.std.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "defs.h"
#include <fcntl.h>
#include <signal.h>
#include <sys/timex.h>
#include "print_time_structured.h"

static ssize_t
fetch_timezone(struct s_arg *arg, unsigned long addr, void *fn_data)
{
	struct timezone tz;

	if (s_umove_verbose(current_tcp, addr, &tz))
		return -1;

	s_insert_d("tz_minuteswest", tz.tz_minuteswest);
	s_insert_d("tz_dsttime", tz.tz_dsttime);

	return 0;
}

static void
s_push_timezone(const char *name)
{
	s_push_addr_type(name, S_TYPE_struct, fetch_timezone, NULL);
}

SYS_FUNC(gettimeofday)
{
	if (entering(tcp)) {
		s_changeable_void("tv");
		s_changeable_void("tz");
	} else {
		s_push_timeval("tv");
		s_push_timezone("tz");
	}
	return 0;
}

#ifdef ALPHA
SYS_FUNC(osf_gettimeofday)
{
	if (entering(tcp)) {
		s_changeable_void("tv");
		s_changeable_void("tz");
	} else {
		s_push_timeval32("tv"):
		s_push_timezone("tz");
	}
	return 0;
}
#endif

SYS_FUNC(settimeofday)
{
	s_push_timeval("tv");
	s_push_timezone("tz");

	return RVAL_DECODED;
}

#ifdef ALPHA
SYS_FUNC(osf_settimeofday)
{
	s_push_timeval32("tv");
	s_push_timezone("tz");

	return RVAL_DECODED;
}
#endif

SYS_FUNC(nanosleep)
{
	if (entering(tcp)) {
		s_push_timespec("req");
		s_changeable_void("rem");
	} else {

		/*
		 * Second (returned) timespec is only significant if syscall
		 * was interrupted.  On success and in case of other errors we
		 * print only its address, since kernel doesn't modify it,
		 * and printing the value may show uninitialized data.
		 */
		if (is_erestart(tcp)) {
			temporarily_clear_syserror(tcp);
			s_push_timespec("rem");
			restore_cleared_syserror(tcp);
		} else {
			s_push_addr("rem");
		}
	}
	return 0;
}

#include "xlat/itimer_which.h"

SYS_FUNC(getitimer)
{
	if (entering(tcp)) {
		s_push_xlat_signed("which", itimer_which, "ITIMER_???");
		s_changeable_void("curr_value");
	} else {
		s_push_itimerval("curr_value");
	}
	return 0;
}

#ifdef ALPHA
SYS_FUNC(osf_getitimer)
{
	if (entering(tcp)) {
		s_push_xlat_signed("which", itimer_which, "ITIMER_???");
		s_changeable_void("curr_value");
	} else {
		s_push_itimerval32("curr_value");
	}
	return 0;
}
#endif

SYS_FUNC(setitimer)
{
	if (entering(tcp)) {
		s_push_xlat_signed("which", itimer_which, "ITIMER_???");
		s_push_itimerval("new_value");
		s_changeable_void("old_value");
	} else {
		s_push_itimerval("old_value");
	}
	return 0;
}

#ifdef ALPHA
SYS_FUNC(osf_setitimer)
{
	if (entering(tcp)) {
		s_push_xlat_signed("which", itimer_which, "ITIMER_???");
		s_push_itimerval32("new_value");
		s_changeable_void("old_value");
	} else {
		s_push_itimerval32("old_value");
	}
	return 0;
}
#endif

#include "xlat/adjtimex_state.h"

static int
do_adjtimex(struct tcb *tcp)
{
	if (s_push_timex() < 0)
		return 0;
	tcp->auxstr = xlookup(adjtimex_state, (unsigned long) tcp->u_rval);
	if (tcp->auxstr)
		return RVAL_STR;
	return 0;
}

SYS_FUNC(adjtimex)
{
	if (entering(tcp)) {
		s_changeable_void("timex");
	} else {
		return do_adjtimex(tcp);
	}
	return 0;
}

#include "xlat/clockflags.h"
#include "xlat/clocknames.h"

const char *
sprintclockname(int clockid)
{
	static char buf[sizeof("MAKE_PROCESS_CPUCLOCK(1234567890,"
		"1234567890 /* CPUCLOCK_??? */)")];
	const char *str;
	ssize_t pos = 0;

#ifdef CLOCKID_TO_FD
# include "xlat/cpuclocknames.h"

	if (clockid < 0) {
		ssize_t pos2 = 0;

		if ((clockid & CLOCKFD_MASK) == CLOCKFD) {
			snprintf(buf, sizeof(buf), "FD_TO_CLOCKID(%d)",
				CLOCKID_TO_FD(clockid));
		} else {
			if (CPUCLOCK_PERTHREAD(clockid))
				pos = snprintf(buf, sizeof(buf),
					"MAKE_THREAD_CPUCLOCK(%d,",
					CPUCLOCK_PID(clockid));
			else
				pos = snprintf(buf, sizeof(buf),
					"MAKE_PROCESS_CPUCLOCK(%d,",
					CPUCLOCK_PID(clockid));

			if ((pos < 0) || ((size_t)pos >= sizeof(buf)))
				return "CLOCK_???";

			pos2 = sprintxval(buf + pos, sizeof(buf) - pos,
				cpuclocknames, clockid & CLOCKFD_MASK,
				"CPUCLOCK_???");

			if ((pos2 < 0) || ((size_t)pos2 >= (sizeof(buf) - pos)))
				return "CLOCK_???";
		}

		return buf;
	}
#endif

	pos = sprintxval(buf, sizeof(buf), clocknames, (unsigned)clockid,
		"CLOCK_???");

	if ((pos < 0) || ((size_t)pos >= sizeof(buf)))
		return "CLOCK_???";

	return buf;
}

SYS_FUNC(clock_settime)
{
	s_push_clockid("clk_id");
	s_push_timespec("tp");

	return RVAL_DECODED;
}

SYS_FUNC(clock_gettime)
{
	if (entering(tcp)) {
		s_push_clockid("clk_id");
		s_changeable_void("tp");
	} else {
		s_push_timespec("tp");
	}
	return 0;
}

SYS_FUNC(clock_nanosleep)
{
	if (entering(tcp)) {
		s_push_clockid("clk_id");
		s_push_flags_int("flags", clockflags, "TIMER_???");
		s_push_timespec("request");
		s_changeable_void("remain");
	} else {
		/*
		 * Second (returned) timespec is only significant
		 * if syscall was interrupted and flags is not TIMER_ABSTIME.
		 */
		if (!tcp->u_arg[1] && is_erestart(tcp)) {
			temporarily_clear_syserror(tcp);
			s_push_timespec("remain");
			restore_cleared_syserror(tcp);
		} else {
			s_push_addr("remain");
		}
	}
	return 0;
}

SYS_FUNC(clock_adjtime)
{
	if (entering(tcp)) {
		s_push_clockid("clk_id");
		s_changeable_void("timex");
	} else {
		return do_adjtimex(tcp);
	}
	return 0;
}

SYS_FUNC(timer_create)
{
	if (entering(tcp)) {
		s_push_clockid("clockid");
		s_push_sigevent("sevp");
		s_changeable_void("timerid");
	} else {
		s_push_d_addr("timerid");
	}
	return 0;
}

SYS_FUNC(timer_settime)
{
	if (entering(tcp)) {
		s_push_d("timerid");
		s_push_flags_int("flags", clockflags, "TIMER_???");
		s_push_itimerspec("new_value");
		s_changeable_void("old_value");
	} else {
		s_push_itimerspec("old_value");
	}
	return 0;
}

SYS_FUNC(timer_gettime)
{
	if (entering(tcp)) {
		s_push_d("timerid");
		s_changeable_void("curr_value");
	} else {
		s_push_itimerspec("curr_value");
	}
	return 0;
}

#include "xlat/timerfdflags.h"

SYS_FUNC(timerfd_create)
{
	s_push_clockid("clockid");
	s_push_flags_int("flags", timerfdflags, "TFD_???");

	return RVAL_DECODED | RVAL_FD;
}

SYS_FUNC(timerfd_settime)
{
	if (entering(tcp)) {
		s_push_fd("fd");
		s_push_flags_int("flags", timerfdflags, "TFD_???");
		s_push_itimerspec("new_value");
		s_changeable_void("old_value");
	} else {
		s_push_itimerspec("old_value");
	}

	return 0;
}

SYS_FUNC(timerfd_gettime)
{
	if (entering(tcp)) {
		s_push_fd("fd");
		s_changeable_void("curr_value");
	} else {
		s_push_itimerspec("curr_value");
	}
	return 0;
}
