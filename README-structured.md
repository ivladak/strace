Structured output is a new approach to emitting syscall information
(arguments and such) from parsers. It provides an API for parsers to use
for saving retrieved information. That information is then being output using
one of the formatters in a formatter-specified way. For now, text and json
formatters are provided.

## TOC

 * TOC
 * [Usage overview](#usage-overview)
 * [Usage patterns](#usage-patterns)
   * [Simple cases](#simple-cases)
   * [Insertion of xlat values](#insertion-of-xlat-values)
   * [Insertion of structures and arrays](#insertion-of-structures-and-arrays)
   * [Insertion of changeable arguments](#insertion-of-changeable-arguments)
 * [API description](#api-description)
   * [Arguments](#arguments)
   * [Types](#types)
   * [Calls](#calls)
 * [Internal API](#internal-api)
   * [`s_syscall`](#s_syscall)
   * [Calls](#calls-1)
 * [Extension use cases](#extension-use-cases)
   * [Working with `s_arg`](#working-with-s_arg)
   * [How to add a type of a new kind](#to-add-a-type-of-a-new-kind)
   * [How to add a type of exiting kind](#to-add-type-of-exiting-kind-eg-num)
   * [How to add a formatter](#to-add-a-new-formatter)

## Usage overview

Each syscall has some associated information (stored in struct tcb) and a list
of arguments. Syscall parser should process available syscall information
(args, stored in `tcb->u_arg[]` / `u_ext_arg[]`, and on exiting —
return value, `tcp->u_rval`) and store it in the `tcb->s_syscall` structure.
In order to ease and steamline this task an API is provided. In short, current
approach to using the API by syscall parsers is the following:
 * Push all arguments on entering (`entering(tcp) == true`).
 * Update all changeable arguments on exiting (`exiting(tcp) == true`).

Aside from syscall data, `s_syscall` structure also contains some state
information, used for simplifying task of adding data to `s_syscall`:

* Changeable arguments queue. Syscall arguments can be marked as changeable
  after being added; it is then used on exiting. Insertion of arguments
  on exiting means update of changeable arguments' values.

* Current argument (`cur_arg`) - index in the `u_arg` array of the argument
  that should be used next. On syscall entering it is incremented contiguously,
  on syscall exiting it is iterated over changeable arguments.
  Structured output engine is aware of argument sizes; e.g. when a long long
  argument is added on LP32 architectures, `cur_arg` is advanced correctly;
  it also behaves right around architecture-specific alignment and sizing
  issues - see `getllval` and `s_syscall_cur_arg_advance` for details.

* Insertion stack. When compound data chunks (such as structs and arrays)
  are being stored, it is handy to avoid passing any sort of handler
  of the internal representation of currently filled structure each time
  a new piece of data is added. Therefore` s_syscall` maintains a stack
  of active structs/arrays; this way, when insertion is performed,
  the data is appended to the structure which lies on top of the insertion stack.

  This allows for code like this:

        /* Add a new structure to the insertion stack */
        s_insert_struct("rusage");
        /* Now all the following s_insert_* calls will insert into this struct */
        /* Append a field to the rusage structure */
        s_insert_llu("ru_maxrss", ru.ru_maxrss);
        /* Push another structure into the insertion stack */
        s_insert_struct("ru_utime");
        /* Append a field to the ru_utime structure */
        s_insert_llu("tv_sec", ru.ru_utime.tv_sec);
        /* Append another field to the same structure */
        s_insert_llu("tv_usec", ru.ru_utime.tv_usec);
        /* Pop the last inserted struct from the insertion stack */
        s_struct_finish();
        /* Now all following calls will insert to struct rusage again */
        /* Append a field to the rusage structure */
        s_insert_llu("ru_msgsnd", ru.ru_msgsnd);
        /* Pop struct rusage from the insertion stack */
        s_struct_finish();

There are two main kinds of insertion functions:
 * `s_insert_*(name, value)` - insert specified data at current insertion point.
 * `s_push_*(name)` - "magic" function, insert data pointed by `cur_arg` to
   the top-level `s_syscall` argument list, resetting insertion stack
   in the process. This makes writing simple syscall parsers easy
   and straighforward:

        s_push_fd("fd");
        s_push_lld("offset");
        s_push_lu("count");

   (from readahead.c)

   By combining this with the concept of changeable arguments we can achieve this:

        if (entering(tcp)) {
                s_push_fd("out_fd");
                s_push_fd("in_fd");
                s_push_lu_addr("offset");
                s_changeable();
                s_push_lu("count");
        } else {
                if (!syserror(tcp) && tcp->u_rval)
                        s_push_lu_addr("offset");
        }

   (from sendfile.c)

   On exiting `s_push_lu_addr("offset")` inserts a value pointed by the same
   argument that was used for `s_push_lu_addr("offset")` on entering.
   Matching is done via `s_changeable()` call; it marks previously inserted
   argument as changeable. The insertion of an argument on exiting assumes
   that it is an updated value for the corresponding changeable argument.


## Usage patterns

This chapter tries to summarize common usage patterns occurring during usage and
extension of structured API.

### Simple cases

In case argument semantics and representation fits one of existing types, one
can just call `s_push_<type>` function. Most of `s_push_*` functions get argument
name as first argument - it is not used for top-level arguments in text
formatter, but present in structure field names and in JSON formatter, so it is
wise to fill this field properly (in accordance with manpages or syscalls.h
syscall definition).

In case the value is not passed directly via syscall's arguments but rather via
pointer, `s_push_*_addr()` call should be used instead.

In case value needs special treatment before storing it in syscall's argument
list, one can follow the next scheme (one example of this pattern is present in
fanotify.c, `fanotify_mask` syscall parser):

    unsigned long long val = s_get_cur_arg(S_TYPE_llu);
    <process val>
    s_insert_llu("argument_name", val);

(unsigned long long and `S_TYPE_llu` types are chosen arbitrary in this example
for the demonstration purposes and should be replaced with actual types
in the actual code)

### Insertion of xlat values

Due to the vast diversity of usage and presentation of xlat values it is kinda
difficult to give an universal set of recipes. Nevertheless some common use
cases are covered below.

In the simplest case, when the argument value should be one of predefined set
of named values or composition of simple flags, one can simply use `s_push_*`:

    s_push_xlat_int("cmd", xlat_list, "CMD_???");
    s_push_flags_64("flags", flags_xlat, "FLAGS_???");

As noted previously, this set of calls has rather perverted naming convention:
 * `int` - unsigned int, unknown values printed in hexadecimal
 * `long` - unsigned long, unknown values printed in hexadecimal
 * `64` - unsigned long long, unknown values printed in hexadecimal
 * `uint` - unsigned int, unknown values printed in decimal
 * `signed` - signed int, unknown values printed in decimal

In some cases, usually as part of complex command decoding, the only thing is
needed is to insert some value with its symbolic name. In these cases it is
handy to use `s_{push,insert}_xlat_*` with `NULL` xlat parameter:

    s_push_xlat_long("arg2", NULL, "PR_SET_PTRACER_ANY");

(from prctl.c)

It is also quite useful in combination with other parts of complex value, as
would be shown further.

Sometimes, argument unconditionally and precisely consists of an xlat part and
flags part. In this case it is sometimes sensible to use `s_push_xlat_flags_*`:

   s_push_xlat_flags("swapflags", swap_flags, NULL,
           SWAP_FLAG_PRIO_MASK, "SWAP_FLAG_???", NULL, false, 0);

(from swapon.c)

It worth describing what are all these arguments are about:

 * First argument is argument name, as usual.
 * Next two arguments are for flags xlat and value xlat, respectively. Note
   that in the above case, value xlat is NULL, it means that no string lookup
   would be performed for the value, it would be emitted as is, with its default
   value.
 * Then, there is mask arguments. It splits value from flags - value is the
   part which matches the mask and flags is all that left.
 * Next two arguments are for flags and value default values, respectively. It
   worth noting that in this example both xlat and default values are `NULL`,
   which leads to simply printing of the value as is, which make sense since
   this is just swap priority.
 * Seventh argument controls order in which value and flags are shown. Sinceit
   is called `preceding_xlat`, setting it to true makes value being pinted in
   front of flags, and setting it to false leads to the reverse - first flags,
   then the value.
 * Eighth argument controls value scaling factor, more about it a bit later.

Due to its increased complexity and rigidness it's hard to recommend this call
as all-round solution, but there are quite of examples where it fits perfectly.

When standard patterns do not apply, the more straightforward way then is to
retrieve value (via `s_get_cur_arg`), process it and add to `s_syscall` via set of
`s_insert_*` and `s_append_*` calls:

    s_insert_flags_int(name, cap_mask0, lo, "CAP_???");
    s_append_flags_int_val(name, cap_mask1, hi, "CAP_???");

(from capability.c)

    s_insert_xlat_int(name, open_access_modes, flags & 3, NULL);
    s_append_flags_int_val(name, open_mode_flags, flags & ~3, NULL);

(from open.c)

Note that for `s_append_*` `_val` suffix is used in order to distinguish from the
variant where argument pointed by `cur_arg` is appended (kinda weird decision,
isn't it?)

When argument has symbolic values to which some shift should be applied,
`*_scaled` versions of aforementioned functions allow signify it:

    s_insert_xlat_uint_scaled("call", NULL, version, NULL, 16);

(from ipc.c)

The last argument is a shift amount. Note that it can be positive or negative -
in case of negative shift value, lookup and symbolic value printing performed
using non-scaled value, but in case raw value is printed, it would be in form
`descaled_value<<scale`. For example:

    s_insert_xlat_int_scaled("name", xlat_vals, 16, "VAL_???", 3) =>
        VAL_SYMBOLIC_2<<3
    s_insert_xlat_int_scaled("name", xlat_vals, 16, "VAL_???", -3) =>
        VAL_SYMBOLIC_16
    s_insert_xlat_int_scaled("name", xlat_vals, 24, "VAL_???", 3) =>
        3<<3 /* VAL_??? */
    s_insert_xlat_int_scaled("name", xlat_vals, 24, "VAL_???", -3) =>
        3<<3 /* VAL_??? */

### Insertion of structures and arrays

Due to variations in structure organization, structured API provide only basic
interfaces for creating structures, more like of callback-driven framework. As
a result, fetching of specific structures is implemented separately in various
parts of code, providing more user-friendly `s_insert_*`/`s_push_*` interface to
syscall parsers (see print_statfs.c, print_time_structured.c for examples of
such interfaces).

Usually, structs and arrays are available via pointer, and in case of failure of
obtaining them, only address is printed. So, the framework handles creation of
`s_addr` argument and nested `s_arg` of specific type along with its removal in
case of failure.More specifically, there are two variant of API calls provided:
 * Thin wrapper, which handles only s_arg struct creation and removal, fetching
   data and filling of `s_arg` with it is up to callback.
 * Thick wrapper, which also fetches data itself. The drawback is that it
   should know in advance how many data to fetch, which can complicate things
   in case of personality-dependent structure size.

It worth noting that in case of filling structure with data, the structure to
fill is placed on the top of insertion stack (handy, isn't it?), so callback
implementation just consists of several `s_insert_*` calls which put data into
structure.

Several examples:

Non-personality-dependent structure:

    static ssize_t
    print_rlimit64(struct s_arg *arg, unsigned long addr, void *fn_data)
    {
            struct rlimit_64 {
                    uint64_t rlim_cur;
                    uint64_t rlim_max;
            } rlim;

            if (s_umove_verbose(current_tcp, addr, &rlim))
                    return -1;

            s_insert_rlim64("rlim_cur", rlim.rlim_cur);
            s_insert_rlim64("rlim_max", rlim.rlim_max);

            return sizeof(rlim);
    }

    static void
    s_insert_rlimit64_addr(const char *name, unsigned long addr)
    {
            s_insert_addr_type(name, addr, S_TYPE_struct, print_rlimit64, NULL);
    }

    static void
    s_push_rlimit64_addr(const char *name)
    {
            s_push_addr_type(name, S_TYPE_struct, print_rlimit64, NULL);
    }

(from resource.c)

Several things to note here:
 * `umove_or_printaddr` is replaced with `s_umove_verbose` which does the same
   checks (`syserror`, `verbose`, ...), but does not print anything upon failure.
 * `fetch_fill` callbacks returns amount of data read or -1 on failure.
 * `fetch_fill` callback takes pointer to `s_arg`, addr to fetch from and
   callback-specific data (which is provided as a last argument of
   `s_*_addr_type`)

In case of complex, nested structures callback can use
`s_insert_struct`/`s_struct_finish` and either fill internals itself or call
another callback:

    static ssize_t
    fetch_fill_rusage(struct s_arg *arg, unsigned long addr, void *fn_data)
    {
            rusage_t ru;

            if (s_umove_verbose(current_tcp, addr, &ru))
                    return -1;

            s_insert_struct("ru_utime");
            s_insert_llu("tv_sec",  widen_to_ull(ru.ru_utime.tv_sec));
            s_insert_llu("tv_usec", widen_to_ull(ru.ru_utime.tv_usec));
            s_struct_finish();

            <...>
    }

(from printrusage.c)

    static ssize_t
    fetch_fill_itimerspec(struct s_arg *arg, unsigned long addr, void *fn_data)
    {
            struct s_struct *s;
            timespec_t t[2];

            if (s_umove_verbose(current_tcp, addr, &t))
                    return -1;

            s = s_insert_struct("it_interval");
            fill_timespec_t(&s->arg, t, sizeof(t[0]), fn_data);
            s_struct_finish();
            s = s_insert_struct("it_value");
            fill_timespec_t(&s->arg, t + 1, sizeof(t[1]), fn_data);
            s_struct_finish();

            return sizeof(t);
    }

(from timespec.c)

Note that the latter handler calls `fill_timespec_t`, which has quite different
argument arrangement. It is so-called fill callback, which is used alongside
thick wrapper. It is simpler in terms of implementation (lacks logic related to
umove), but callee should know amount of data to read from tracee:

    static int
    fill_cap_user_header(struct s_arg *arg, void *buf, unsigned long len,
            void *fn_data)
    {
            cap_user_header_t *h = buf;

            s_insert_xlat_int("version", cap_version, h->version,
                    "_LINUX_CAPABILITY_VERSION_???");
            s_insert_d("pid", h->pid);

            return 0;
    }

    <...>

    s_push_addr_type_sized("hdrp", sizeof(cap_user_header_t),
            S_TYPE_struct, fill_cap_user_header, &version);

(from capability.c)

Note that fill callback is also the only callback type which is possible to use
with `s_{insert,push}_array_type` wrappers:

    static int
    fill_group(struct s_arg *arg, void *buf, size_t len, void *fn_data)
    {
            S_ARG_TO_TYPE(arg, num)->val = *((uid_t *)buf);

            return 0;
    }

    <...>

    s_push_array_type(name, len, sizeof(uid_t), S_TYPE_gid, fill_group,
            NULL);

(from uid.c)

Note that in the last example `s_arg` itself is modified (since it is not
container-type like struct, but rather plain numeric type), and in order to do
this, it is cast to specific argument type with `S_ARG_TO_TYPE`.

The last thing that should be noted regarding handling of address-pointed types
is multiple personality support.  For now, best practice is wrap
`s_insert_*`/`s_push_*` calls with `MPERS_PRINTER_DECL` and call callbacks from them.
It allows for proper type size calculation even in case fill callback is used.

    MPERS_PRINTER_DECL(void, s_insert_rusage, const char *name,
            unsigned long addr)
    {
            s_insert_addr_type(name, addr, S_TYPE_struct, fetch_fill_rusage,
	            NULL);
    }

    MPERS_PRINTER_DECL(void, s_push_rusage, const char *name)
    {
            s_push_addr_type(name, S_TYPE_struct, fetch_fill_rusage, NULL);
    }

(from printrusage.c)

### Insertion of changeable arguments

* `s_changeable` converts last inserted arg to changeable.
* `s_changeable_void` inserts a changeable argument
  that does not make sense on entering.

All changeable arguments should either be marked on entering with `s_changeable`
or be empty on entering (and therefore inserted with `s_changeable_void`).

## API description

### Arguments

Each argument has multiple required fields (they are defined in struct `s_arg`,
which is base for all argument structures):
 * `type` - describes what this argument is and how should it be handled.
 * `name` - name of the argument (or field name, if this argument is part of a
   structure).
There are also several internally used fields, like entry records for
argument list and changeable argument list.

### Types

Each argument has a type associated with it. Types have multiple properties:
 * Size - symbolises argument size. Most useful for numeric arguments.
  * TODO: add special size for arguments which can't be stored as is
 * Sign - whether argument is signed, unsigned or have default signedness (for
   chars).
  * TODO: do we really need this? Maybe combine it with format?
 * Format - relates to the way argument should be formatted
 * Kind - way how argument is stored. Mostly affects the way to what `s_arg`
   struct should be cast.

Currently the following types are present (prefixed with `S_TYPE_`):
 * Numeric types. These are various types which are backed by some numeric
   value (`uid_t`, `mode_t`, `dev_t`, ...). Various formats usually used
   in order to handle appropriate special values (-1 for `uid_t`, 
   `0xFFFFFFFFFFFFFFFF` for `rlim64`, `SIG_*` for `sa_handler` and so on)
   or special human-friendly formatting (as in case of `mode_t` or `dev_t`).
   The full list of currently supported numeric formats is as follows:

  * Simple numbers. Identified by printf format specifier:

	`d`, `ld`, `lld` (signed, decimal integer, long, long long),

	`x`, `lx`, `llx` (unsigned hexidecimal),

	`u`, `lu`, `llu` (unsigned decimal),

	`o`, `lo`, `llo` (unsigned octal).

  * `uid` (`uid_t`), `gid` (`gid_t`), `uid16` (`uid16_t`), `gid16` (`gid16_t`)
  * `time` (`time_t`), `umask`, `umode_t`, `mode_t`, `short_mode_t`, `dev_t`
  * `signo` (`signum` argument of some signal-related syscalls;
	appropriate field of `siginfo` struct).
  * `scno` (syscall ID constant)
  * `wstatus` (wait status)
  * `rlim32`/`rlim64` (32/64 bit resource usage values with handling
	of unlimited special value).
  * `sa_handler` (handling of `SIG_DFL`/`SIG_IGN`/`SIG_ERR` values)
 * `addr` - address type. Can hold pointer to an argument representing value pointed by
   the address.
  * `S_TYPE_ptrace_uaddr` - used for representing address in tracee's address
    space in the form <base address>+<offset> where <base address> is one of
    tracee's addresses stored in registers (handled by `print_user_offset_addr`
    in process.c).
 * `str` - string type. Allows storing of various strings (NUL-terminated or not,
   depending on context) and paths.
 * `xlat` type. Used for storing numeric values associated with some string
   constants. Numeric value can be associated with one of string constant as is
   (open modes, command ID's, etc), or interpreted as set of flags, each being
   associated, in turn, with certain numeric value. Since this relation can be
   quite complex and non-trivial, xlat value is stored as a chain, each part of
   it having its distinct value, set of string constants and way of association
   between value and string constant set. It is also worth noting that due to
   its nature, xlat values hav most complex and non-trivial API comparing to
   other types. It is also one of two types which have special support
   functions for aiding its formatting (`s_process_xlat` in this case).
  * Xlat types has suffixes which determines its output format: `xlat` (unsigned
    hexadecimal), `xlat_l` (long unsigned hexadecimal), `xlat_ll` (long long
    unsigned hexadecimal), `xlat_d`/`xlat_ld`/`xlat_lld` (signed decial),
	`xlat_u` (unsigned decimal).
 * `sigmask` - signal mask. Due to the fact it is bitmask of quite arbitrary
   size (up to 128 bits in case of MIPS architecture), it can't be stored
   as simple numeric type. Other notable thing is that this is the other type
   which has special format processing helper function, 1s_process_sigmask`.
  * Note that due to various reasons, API calls related to this type are
    contained in separate header, `structured_sigmask.h`.
 * `struct` type. Container for other arguments.
 * `array` type is another argument container.
 * `ellipsis`. Utility type, used in case reading of data is limited externally
   (by max_strlen setting, for example) and used for expressing that data is
   read not in full.
 * `changeable`. Special two-item container type. Stores arguments related to
   syscall entering and exiting.

### Calls

Overall naming scheme:
 * `s_insert_*` - insert provided value to current insertion point.
  * Possible types: all of them
  * Note that for types  that can't be usually inserted as is (strings, most
    notably), `s_insert_*` means `s_insert_*_addr`.
  * Note that currently xlat/flags have insane and illogical naming convention
    (mostly impacted by legacy function names):
	`s_insert_{xlat,flags}_{int,uint,signed,long,64}`, where:
   * `int` states for unsigned 32-bit hexadecimal
   * `uint` states for unsigned 32-bit decimal
   * `signed` states for signed 32-bit decimal
   * `long` states for unsigned long (current_wordsize) hexadecimal
   * `64` states for unsigned long long hexadecimal
   * TODO: make it sane
 * `s_insert_*_addr` - insert value pointed by address to current insertion
   point. Note that possibility of retrieval of data by pointer is controlled
   by verbosity level in most cases (see `s_umoven_verbose` below).
  * Possible types: all of them, except ellipsis and strings/paths (latter are
    inserted via `s_insert_{str,path}` calls).
 * `s_push_*` - reset insertion stack, push `cur arg` as a (top-level) value of
   provided type, advance `cur_arg` in accordance with type size.
  * Possible types: all numeric types, xlat, addr.
  * For string/path, array/struct, sigmask `s_push_*` means `s_push_*_addr`.
  * There is also special `s_push_empty` call, which induces advancement of
    `cur_arg` without inserting any argument. It is usually useful after
    inserting complex set of xlat values derived from some `cur_arg` value (see
    ipc.c for an example).
 * `s_push_*_addr` - reset insertion stack, push (top-level) value referenced by
   address provided in cur arg.
  * Possible types: all numeric types, xlat, addr.
  * For string/path, array/struct, sigmask use `s_push_*` instead.

Additional calls:
 * `s_get_cur_arg` - returns value pointed by cur arg and advances `cur_arg` (it
   is. in fact, `s_syscall_cur_arg_advance` with simpler name and interface).
 * `s_insert_addr_arg` - used for inserting argument of addr type with prepared
   `s_arg` to which it points. See `s_insert_addr_addr`, `s_insert_string` for
   examples.
 * `s_insert_addr_type`, `s_insert_addr_type_sized`, `s_push_addr_type`,
   `s_push_addr_type_size`, `s_insert_array_type`, `s_push_array_type` - wrappers for
   insertion of some data (usually structure or array) accessible by pointer.
   One should describe `fetch_fill_callback` (which copies data from tracee's
   memory and then fills prepared argument with it) or `fill_callback` (which is
   responsible only for filling) and provide it to the appropriate wrapper. See
   printrusage.c, print_time_structured.c, io.c for examples.
  * The need for two various callbacks is due to the fact that sometimes there
    is difficult logic behind determining amount of data which should be
    copied. If this is not a big concern, then `s_{insert,push}_addr_type_sized`
    and `s_{insert,push}_array_type` are preferred.
  * See below for explanation how to implement these callbacks and use them.
 * `s_changeable` - marks last inserted argument as changeable.
 * `s_changeable_void` - insert empty (with no entering or exiting argument)
   changeable argument. Useful for out arguments, for which value on entering
   has no sense.
  * TODO: Consider renaming to `s_push_changeable` or smth
 * `s_struct_finish`, `s_array_finish` - upon insertion, struct/array pushes itself
   on top of insertion stack. When filling of struct/array is done, it should
   be removed from insertion stack using one of these calls.
 * `s_append_{xlat,flags}_{int,uint,signed,long,64}`,
   `s_append_{xlat,flags}_{int,uint,signed,long,64}_val` - As mentioned
   previously, xlat value, in fact, maintains a chain of values, each
   representing different part of complex xlat value. These function are used
   to append new parts to this chain. As usual, last inserted argument is used
   for appending.
 * `s_insert_str_val` - Usually, strings are accessed by pointers. In some rare
   cases, character arrays are embedded in structure, so there is special call
   for these special cases.
 * `s_push_sigmask_val` - Generally speaking, sigmask may be larger than long
   long type, so it should be passed by pointer (or as an array). Despite this,
   there are several syscalls in which sigmask is passed by value. This special
   call has been engineered specifically for these cases.
 * `s_insert_lld_lu_pair` - this one function is specially crafted for handling
   special case of passing long long value via pair of arguments, as it is done
   in `preadv`/`pwritev` syscalls. For now, only one varian of this function is
   defined (long long signed decimal from pair of long unsigned values), but
   one can easily generate additional variant using `DEF_PUSH_PAIR` macro in
   structured-inlines.h
 * `s_insert_pathn`, `s_push_pathn` - usually, path name is limited to `PATH_MAX`
   bytes. When more strict limit is known, one can use these functions in order
   to provide it.

## Internal API 

### `s_syscall`

Each tcb has an `s_syscall` (of type `struct s_syscall`) associated with it.
`s_syscall` stores the following data:
 * Pointer to `struct tcb` it belongs to.
 * `cur_arg` and `last_arg`. The first one is used during
   `s_syscall_cur_arg_advance` and the second one is used by `s_arg_insert`.
   Thus, they are mostly used internally and are usually not accessed directly.
 * `last_changeable` is also used by `s_arg_insert`/`s_syscall_cur_arg_advance`.
 * `args`, `changeable_args` - list of all (top-level) arguments and list of all
   changeable arguments.
 * type - `s_syscall` structure is also used for storing signal data.
 * `insertion_stack` - stack of pointers to s_struct's arg queues in order to
   allow insertion of data without explicit insertion place providing.

`s_syscall` also has a set of calls associated with it; they are described in the
internal API section.

### Calls

These calls are defined in structured.c and declared in structured.h except in
cases it is noted otherwise.

 * Argument creation and manipulation
  * `s_arg_new`, `s_arg_free` - basic constructor and destructor for argument item.
    Constructor receives argument type as a parameter, which is used for proper
    memory allocation (allowing to simply cast to the needed type when needed).
    Destructor handles freeing of type-specific owned data. Note that
	`s_arg_new` does not perform any additional work regarding initialisation of
    type-specific field, it assumes that callee does that; use `s_arg_new_init` in
    case you need some generic constructor producing usable argument instances.
  * `s_*_new` (`s_num_new`, `s_str_new`, `s_str_val_new`, `s_addr_new`,
	`s_xlat_new`, `s_ellipsis_new`, `s_changeable_new`, `s_sigmask_new`
	(`structured_sigmask`)) -
    type-specific constructors, which provide pointer to argument of specific
    type and take type-specific set of arguments.
  * `s_arg_new_init` - special flavor of s_arg_new which performs type-specific
    initialisation, so argument is returned in non-conflicted state. This call
    (and not `s_arg_new`) should be used externally as a type-generic way for
    creating new argument instances.
  * `s_*_new_and_insert` (where `*` is one of `num`, `addr`, `xlat`, `struct`,
	`ellipsis`, `changeable` or `sigmask` (`structured_sigmask`)) - wrappers for creating new
    argument of specific type and inserting it at the current insertion point.
  * `s_arg_equal` - argument equality check function. Most notable use is for
    determining whether changeable argument has been changed.
 * Handling type-specific peculiarities
  * `s_xlat_append` - appends new xlat argument to existing one (which should be
    last inserted item at the current insertion point). If last item is not
    xlat, new xlat argument inserted at the insertion point. On level of
    addressing is resolved (e.g. if last element is addr argument containing
    xlat as a value, then it would be successfully appended with the value
    provided).
  * `s_struct_set_aux_str`, `s_struct_set_own_aux_str` - some structures have very
    special representation (BPF statements in seccomp.h is a good example). In
    order to handle these cases, `aux_str` field is introduced where arbitrary
    structure representation can be stored. Sometimes this specific
    representation is a static string.
 * Insertion stack handling
  * `s_struct_enter` - places provided s_struct onto the to of insertion stack,
    so all the following insertion calls would append arguments to the argument
    list of this struct.
  * `s_syscall_insertion_point` - provides current insertion point. If there are
    no active structures in the internal insertion stack storage present, it
    returns pointer to the s_sysscall's argument queue (so, essentially,
    `s_syscall`'s argument queue is always at the base of insertion stack).
  * `s_syscall_pop` - removes top element from the insertion stack.
  * `s_syscall_pop_all` - removes all elements from the insertion stack. Next
    insertion calls will insert argument to s_syscall's argument queue.
 * Syscall structure handling
  * `s_syscall_new` - create new struct `s_syscall` and connect it to the
	`struct tcb` passed.
  * `s_syscall_free` - destroy `struct s_syscall` along with all arguments which
    have been added to it.
 * Argument insertion handling
  * `s_arg_insert` - sort of magic function. Normally, it adds provided argument
    to the current insertion point of provided syscall. But also (in case
    non-negative `force_arg` is passed or insertion point is top-level) it writes
    to the argument argument index associated with it and inserts argument to
    the changeable argument queue in case it is changeable. Moreover, on
    exiting, instead of adding to the syscall, it adds to appropriate exiting
    field of "current" changeable argument. Along with `s_last_is_changeable` it
    enables simple semantics described above - "just push the arguments and
    mark them as changeable on entering, push updated changeable argument on
    exiting".
  * `s_last_is_changeable` - replaces last argument being added (more specially,
    last argument in argument list related to current insertion point - this
    means that in order to mark added array/struct as changeable, it should be
    done immediately after `s_{array,struct}_finish`) with changeable argument.
  * `s_syscall_cur_arg_advance` - retrieves argument value pointed by `cur_arg`
	in accordance with type size (mpers-aware - returns 4 or 8 bytes for long,
    combines (using `getllval`) values from two arguments in order to obtain
    64-bit value on 32-bit platforms) requested and increases `cur_arg`
    accordingly.
 * Processing helpers
  * `s_process_xlat`, `s_process_sigmask` (`structured_sigmask`) - this helpers
    iterate over values packed in argument values and calls callback for each
    primitive value. For xlat, it perform a call for each flag or xlat value;
    in case of sigmask it performs a call upon each bit.

## Extension use cases

### Working with `s_arg`

 * To convert from `s_arg` to a specific `s_*`:
  * Compile-time: `S_ARG_TO_TYPE`
  * Runtime: `s_arg_to_type`
 * To convert from `s_*` to `s_arg`:
  * Compile-time: `S_TYPE_TO_ARG`
  * Runtime: `&(p->arg)`

### To add a type of a new kind
  * Add it to the `s_type_kind` enum
  * Add all appropriate type definitions to `enum s_type`
  * Add a related `s_*` struct defintion
  * Add it to `S_TYPE_FUNC` definition
  * Add it to `s_arg_new_init`, `s_arg_equal` and `s_arg_free`
  * Add related `s_insert_*`/`s_push_*` to structured-inlines.h
  * Add it to formatters

### To add type of exiting kind (e.g. num)
  * Add a new format definition to `s_type_format`
  * Add a type definition to `s_type enum`
  * Add related `s_insert_*`/`s_push_*` to structured-inlines.h
        (for num it would be just another `DEF_PUSH_INT`)
  * Add it to formatters

### To add a new formatter
 * Create an instance of `struct s_printer` containing function pointers,
   add a pointer to `s_printers` array in structured.c
 * To format xlat and sigmask values: there are corresponding
   `s_process_{xlat,sigmask}` helper functions which take callback function
   as one of arguments.
