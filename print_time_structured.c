#include "defs.h"
#include "print_time_structured.h"

#ifdef ALPHA

typedef struct {
	int tv_sec, tv_usec;
} timeval32_t;

static const char time_fmt[] = "{%jd, %jd}";

static int
s_fill_timeval32(struct s_arg *arg, void *buf, long len, void *fn_data)
{
	timeval32_t *t = buf;

	s_insert_d("tv_sec", t->tv_sec);
	s_insert_d("tv_usec", t->tv_usec);

	return 0;
}

void
s_insert_timeval32_addr(const char *name, long addr)
{
	s_insert_addr_type_sized(name, sizeof(timeval32_t), addr, S_TYPE_struct,
		fill_timeval32_t, NULL);
}

void
push_timeval32_pair(struct tcb *tcp, const char *name)
{
	s_push_array_type(name, 2, sizeof(timeval_t), S_TYPE_struct,
		fill_timeval32_t, NULL);
}

static ssize_t
s_fetch_fill_itimerval32(struct s_arg *arg, long addr, void *fn_data)
{
	ssize_t ret = s_insert_addr_type_sized("it_interval", sizeof(timeval32_t),
		addr, S_TYPE_struct, fill_timeval32_t, NULL);
	if (ret < 0) return ret;
	return ret + s_insert_addr_type_sized("it_value", sizeof(timeval32_t),
		addr + sizeof(timeval_t), S_TYPE_struct, fill_timeval32_t, NULL);
}

const char *
sprint_timeval32(struct tcb *tcp, const long addr)
{
	timeval32_t t;
	static char buf[sizeof(time_fmt) + 3 * sizeof(t)];

	if (!addr) {
		strcpy(buf, "NULL");
	} else if (!verbose(tcp) || (exiting(tcp) && syserror(tcp)) ||
		   umove(tcp, addr, &t)) {
		snprintf(buf, sizeof(buf), "%#lx", addr);
	} else {
		snprintf(buf, sizeof(buf), time_fmt,
			 (intmax_t) t.tv_sec, (intmax_t) t.tv_usec);
	}

	return buf;
}

void
s_push_timeval32(const char *name)
{
	s_push_addr_type_sized(name, sizeof(timeval32_t), S_TYPE_struct,
		s_fill_timeval32, NULL);
}

void
s_push_itimerval32(const char *name)
{
	s_push_addr_type(name, S_TYPE_struct, s_fetch_fill_itimerval32, NULL);
}

#endif /* ALPHA */
