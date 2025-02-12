/* ============================================================================
//
// = LIBRARY
//    ULib - c library
//
// = FILENAME
//    base.c
//
// = AUTHOR
//    Stefano Casazza
//
// ============================================================================ */
 
/*
#define DEBUG_DEBUG
*/

#include <ulib/base/hash.h>
#include <ulib/base/error.h>
#include <ulib/base/utility.h>
#include <ulib/internal/chttp.h>
#include <ulib/base/coder/escape.h>

#ifdef DEBUG
#  define U_INTERNAL_ERROR(assertion,format,args...) \
      if ((bool)(assertion) == false) { \
         u_internal_print(true, \
         "  pid: %.*s\n" \
         " file: %s\n" \
         " line: %d\n" \
         " function: %s\n" \
         " assertion: \"(%s)\" \n" \
         "-------------------------------------\n" \
         format "\n", \
         u_pid_str_len, u_pid_str, \
         __FILE__, \
         __LINE__, \
         __PRETTY_FUNCTION__, \
         #assertion , ##args); \
      }

void u_debug_init(void);
#else
#  define U_INTERNAL_ERROR(assertion,format,args...)
/*
#  undef  U_INTERNAL_TRACE
#  define U_INTERNAL_TRACE(format,args...) u_internal_print(false, format"\n" , ##args);
#  undef  U_INTERNAL_PRINT
#  define U_INTERNAL_PRINT(format,args...) U_INTERNAL_TRACE(format,args)
*/
#endif

void u_debug_at_exit(void);

#include <time.h>
#include <errno.h>

#ifdef HAVE_ENDIAN_H
#  include <endian.h>
#elif defined(HAVE_SYS_ENDIAN_H)
#  include <sys/endian.h>
#endif
#ifdef HAVE_SYS_SYSCALL_H
#  include <sys/syscall.h>
#endif
#ifndef _MSWINDOWS_
#  include <pwd.h>
#  include <sys/uio.h>
#  include <sys/utsname.h>
#endif
/* For TIOCGWINSZ and friends: */
#ifdef HAVE_SYS_IOCTL_H
#  include <sys/ioctl.h>
#endif
#ifdef HAVE_TERMIOS_H
#  include <termios.h>
#endif

/* String representation */
struct ustringrep u_empty_string_rep_storage = {
#ifdef DEBUG
   (void*)U_CHECK_MEMORY_SENTINEL, /* memory_error (_this) */
#endif
#if defined(U_SUBSTR_INC_REF) || defined(DEBUG)
   0, /* parent - substring increment reference of source string */
#  ifdef DEBUG
   0, /* child  - substring capture event 'DEAD OF SOURCE STRING WITH CHILD ALIVE'... */
#  endif
#endif
   0, /* _length */
   0, /* _capacity */
   0, /* references */
  ""  /* str - NB: we need an address (see c_str() or isNullTerminated()) and must be null terminated... */
};

/* Startup */
bool                 u_is_tty;
pid_t                u_pid;
uint32_t             u_pid_str_len;
uint32_t             u_progname_len;
      char* restrict u_pid_str;
const char* restrict u_progpath;
const char* restrict u_progname;

/* Current working directory */
char*                u_cwd;
uint32_t             u_cwd_len;

/* Location info */
uint32_t             u_num_line;
const char* restrict u_name_file;
const char* restrict u_name_function;

/* Internal buffer */
char*    u_buffer;
char*    u_err_buffer;
uint32_t u_buffer_len; /* signal that is busy if != 0 */

/* Time services */
bool   u_daylight;
void*  u_pthread_time; /* pthread clock */
time_t u_start_time;
int    u_now_adjust;   /* GMT based time */
struct tm u_strftime_tm;

struct timeval u_timeval;
struct timeval* u_now = &u_timeval;

const char* u_months[]    = { "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec" };
const char* u_months_it[] = { "gen", "feb", "mar", "apr", "mag", "giu", "lug", "ago", "set", "ott", "nov", "dic" };

/* Services */
int                  u_errno; /* An errno value */
int                  u_flag_exit;
int                  u_flag_test;
bool                 u_recursion;
bool                 u_fork_called;
bool                 u_exec_failed;
char                 u_user_name[32];
char                 u_hostname[HOST_NAME_MAX+1];
int32_t              u_printf_string_max_length;
uint32_t             u_hostname_len, u_user_name_len, u_seed_hash = 0xdeadbeef;
const int            MultiplyDeBruijnBitPosition2[32] = { 0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9 };
const char* restrict u_tmpdir = "/tmp";
const unsigned char  u_alphabet[]  = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const unsigned char  u_hex_upper[] = "0123456789ABCDEF";
const unsigned char  u_hex_lower[] = "0123456789abcdef";

struct uclientimage_info u_clientimage_info;

/* conversion table number to string */
const char u_ctn2s[201] = "0001020304050607080910111213141516171819"
                          "2021222324252627282930313233343536373839"
                          "4041424344454647484950515253545556575859"
                          "6061626364656667686970717273747576777879"
                          "8081828384858687888990919293949596979899";

/**
 * "FATAL: kernel too old"
 *
 * Even if you recompile the code with -static compiler command-line option to avoid any dependency on the dynamic Glibc library,
 * you could still encounter the error in question, and your code will exit with Segmentation Fault error.
 *
 * This kernel version check is done by DL_SYSDEP_OSCHECK macro in Glibc's sysdeps/unix/sysv/linux/dl-osinfo.h
 * It calls _dl_discover_osversion to get current kernel's version.
 *
 * The fix (or hack) is to add the following function in your code and compile your code with -static compiler command-line option.
 *
 * int _dl_discover_osversion() { return 0xffffff; }
 */

__pure const char* u_basename(const char* restrict path)
{
   const char* restrict base;

   U_INTERNAL_TRACE("u_basename(%s)", path)

#ifdef _MSWINDOWS_
   if (u__isalpha(path[0]) && path[1] == ':') path += 2; /* Skip over the disk name in MSDOS pathnames */
#endif

   for (base = path; *path; ++path) if (IS_DIR_SEPARATOR(*path)) base = path + 1;

   return base;
}

__pure const char* u_getsuffix(const char* restrict path, uint32_t len)
{
   const char* restrict ptr;

   U_INTERNAL_TRACE("u_getsuffix(%.*s,%u)", U_min(len,128), path, len)

   U_INTERNAL_ASSERT_POINTER(path)

   // NB: we can have something like 'www.sito1.com/tmp'...

   ptr = (const char*) memrchr(path, '.', len);

   return (ptr && memrchr(ptr+1, '/', len - (ptr+1 - path)) == 0 ? ptr : 0);
}

void u_setPid(void)
{
   static char buffer[10];

   pid_t pid_copy;

   U_INTERNAL_TRACE("u_setPid()")

   u_pid     = getpid();
   u_pid_str = buffer + sizeof(buffer);

   pid_copy = u_pid;

   while (pid_copy >= 10)
      {
      *--u_pid_str = (pid_copy % 10) + '0';

      pid_copy /= 10;
      }

   U_INTERNAL_ASSERT_MINOR(pid_copy, 10)

   *--u_pid_str = pid_copy + '0';

   u_pid_str_len = buffer + sizeof(buffer) - u_pid_str;
}

void u_init_ulib_username(void)
{
   struct passwd* restrict pw;

   U_INTERNAL_TRACE("u_init_ulib_username()")

   pw = getpwuid(getuid());

   if (pw == 0) u__memcpy(u_user_name, "root", (u_user_name_len = 4), __PRETTY_FUNCTION__);
   else
      {
      u_user_name_len = u__strlen(pw->pw_name, __PRETTY_FUNCTION__);

      U_INTERNAL_ASSERT_MAJOR(u_user_name_len,0)

      u__memcpy(u_user_name, pw->pw_name, u_user_name_len, __PRETTY_FUNCTION__);
      }
}

void u_init_ulib_hostname(void)
{
   U_INTERNAL_TRACE("u_init_ulib_hostname()")

   u_hostname[0]  = 0;
   u_hostname_len = 0;

   if (gethostname(u_hostname, sizeof(u_hostname)) != 0)
      {
      char* restrict tmp = getenv("HOSTNAME"); /* bash setting... */

      if ( tmp &&
          *tmp)
         {
         u_hostname_len = u__strlen(tmp, __PRETTY_FUNCTION__);

         if (u_hostname_len > HOST_NAME_MAX) u_hostname_len = HOST_NAME_MAX;

         u__memcpy(u_hostname, tmp, u_hostname_len, __PRETTY_FUNCTION__);

         u_hostname[u_hostname_len] = 0;
         }
      else
         {
#     ifndef _MSWINDOWS_
         FILE* node = (FILE*) fopen("/proc/sys/kernel/hostname", "r");

         if (node)
            {
            (void) fscanf(node, "%255s", u_hostname);

            (void) fclose(node);

            u_hostname_len = u__strlen(u_hostname, __PRETTY_FUNCTION__);
            }
         else
            {
            struct utsname buf;

            if (uname(&buf) == 0)
               {
               u_hostname_len = u__strlen(buf.nodename, __PRETTY_FUNCTION__);

               u__memcpy(u_hostname, buf.nodename, u_hostname_len, __PRETTY_FUNCTION__);

               u_hostname[u_hostname_len] = 0;
               }
            }
#     endif
         }
      }

   if (u_hostname_len == 0)
      {
      u_hostname_len = u__strlen(u_hostname, __PRETTY_FUNCTION__);

      if (u_hostname_len == 0) u__strncpy(u_hostname, "localhost", (u_hostname_len = 9) + 1);
      }
}

void u_getcwd(void) /* get current working directory */
{
   size_t newsize = 256;

   U_INTERNAL_TRACE("u_getcwd()")

loop:
   if (u_cwd) free(u_cwd);

   u_cwd = (char*) malloc(newsize);

   if (getcwd(u_cwd, newsize) == 0 && errno == ERANGE)
      {
      newsize += 256;

      U_WARNING("current working directory need a bigger buffer (%u bytes), doing reallocation", newsize);

      goto loop;
      }

#ifdef _MSWINDOWS_
   u__strcpy(u_cwd, u_slashify(u_cwd, PATH_SEPARATOR, '/'));
#endif

   u_cwd_len = u__strlen(u_cwd, __PRETTY_FUNCTION__);

   U_INTERNAL_ASSERT_MAJOR(u_cwd_len, 0)
   U_INTERNAL_ASSERT_MINOR(u_cwd_len, newsize)
}

__pure int u_getMonth(const char* buf)
{
   int i;

   U_INTERNAL_TRACE("u_getMonth(%s)", buf)

   for (i = 0; i < 12; ++i)
      {
      const char* ptr = u_months[i];

      if ((ptr[0] == u__tolower(buf[0])) &&
          (ptr[1] == u__tolower(buf[1])) &&
          (ptr[2] == u__tolower(buf[2])))
         {
         return i+1;
         }

      ptr = u_months_it[i];

      if ((ptr[0] == u__tolower(buf[0])) &&
          (ptr[1] == u__tolower(buf[1])) &&
          (ptr[2] == u__tolower(buf[2])))
         {
         return i+1;
         }
      }

   return 0;
}

bool u_setStartTime(void)
{
   struct tm tm;
   time_t t, lnow;
   const char* compilation_date;

   U_INTERNAL_TRACE("u_setStartTime()")

   U_INTERNAL_ASSERT_POINTER(u_now)

   /**
    * calculate number of seconds between UTC to current time zone
    *
    *         time() returns the time since the Epoch (00:00:00 UTC, January 1, 1970), measured in seconds.
    * gettimeofday() gives the number of seconds and microseconds since the Epoch (see time(2)). The tz argument is a struct timezone:
    *
    * struct timezone {
    *    int tz_minuteswest;  // minutes west of Greenwich
    *    int tz_dsttime;      // type of DST correction
    * };
    */

   (void) gettimeofday(u_now, 0);

   /**
    * The "hash seed" is a feature to perturb the results to avoid "algorithmic complexity attacks"
    *
    * http://lwn.net/Articles/474365
    */
 
   u_seed_hash = u_random((uint32_t)u_pid ^ (uint32_t)u_now->tv_usec);

#ifdef SYS_getrandom
   if (syscall(SYS_getrandom, &u_seed_hash, sizeof(u_seed_hash), 0) == sizeof(u_seed_hash)) u_seed_hash |= 1;
   else
#endif
   {
#ifndef U_COVERITY_FALSE_POSITIVE /* RESOURCE_LEAK */
   int         fd = open("/dev/urandom", O_CLOEXEC | O_RDONLY);
   if (fd < 0) fd = open("/dev/random",  O_CLOEXEC | O_RDONLY);
   if (fd > 0)
      {
      if (read(fd, &u_seed_hash, sizeof(u_seed_hash)) == sizeof(u_seed_hash)) u_seed_hash |= 1;

      (void) close(fd);
      }
#endif
   }

   /* seed the random generator */

   u_set_seed_random(u_seed_hash >> 16, u_seed_hash % 4294967296);

   /* initialize time conversion information */

   tzset();

   /**
    * The localtime() function converts the calendar time to broken-time representation, expressed relative
    * to the user's specified timezone. The function acts as if it called tzset(3) and sets the external
    * variables tzname with information about the current timezone, timezone with the difference between
    * Coordinated Universal Time (UTC) and local standard time in seconds, and daylight to a non-zero value
    * if daylight savings time rules apply during some part of the year. The return value points to a
    * statically allocated struct which might be overwritten by subsequent calls to any of the date and time
    * functions. The localtime_r() function does the same, but stores the data in a user-supplied struct. It
    * need not set tzname, timezone, and daylight
    *
    * This variable (daylight) has a nonzero value if Daylight Saving Time rules apply. A nonzero value does
    * not necessarily mean that Daylight Saving Time is now in effect; it means only that Daylight Saving Time
    * is sometimes in effect.
    *
    * This variable (timezone) contains the difference between UTC and the latest local standard time, in seconds
    * west of UTC. For example, in the U.S. Eastern time zone, the value is 5*60*60. Unlike the tm_gmtoff member
    * of the broken-down time structure, this value is not adjusted for daylight saving, and its sign is reversed.
    * In GNU programs it is better to use tm_gmtoff, since it contains the correct offset even when it is not the latest one.
    */

   (void) localtime_r(&(u_now->tv_sec), &u_strftime_tm);

#ifdef TM_HAVE_TM_GMTOFF
   u_daylight = (daylight && (timezone != -u_strftime_tm.tm_gmtoff));
#endif

   /**
    * The timegm() function converts the broken-down time representation, expressed in Coordinated Universal
    * Time (UTC) to calendar time
    */

   lnow = timegm(&u_strftime_tm);

   u_now_adjust = (lnow - u_now->tv_sec);

   U_INTERNAL_PRINT("u_now_adjust = %d timezone = %ld daylight = %d u_daylight = %d tzname[2] = { %s, %s }",
                     u_now_adjust,     timezone,      daylight,     u_daylight,     tzname[0], tzname[1])

   U_INTERNAL_ASSERT(u_now_adjust <= ((daylight ? 3600 : 0) - timezone))

   /* NB: check if current date is OK (>= compilation_date) */

   compilation_date = __DATE__; /* Dec  6 2012 */

// (void) memset(&tm, 0, sizeof(struct tm));

   tm.tm_min   = 0;
   tm.tm_hour  = 0;
   tm.tm_mday  =       atoi(compilation_date+4);
   tm.tm_mon   = u_getMonth(compilation_date)   -    1; /* tm relative format month - range from 0-11 */
   tm.tm_year  =       atoi(compilation_date+7) - 1900; /* tm relative format year  - is number of years since 1900 */
   tm.tm_sec   = 1;
   tm.tm_wday  = 0; /* day of the week */
   tm.tm_yday  = 0; /* day in the year */
   tm.tm_isdst = -1;

   t = mktime(&tm); /* NB: The timelocal() function is equivalent to the POSIX standard function mktime(3) */

   U_INTERNAL_PRINT("lnow = %ld t = %ld", lnow, t)

   if (lnow >= t ||
       (t - lnow) < U_ONE_DAY_IN_SECOND)
      {
      u_start_time = lnow; /* u_now->tv_sec + u_now_adjust */

      /**
       * The mktime() function modifies the fields of the tm structure as follows: tm_wday and tm_yday are set to values
       * determined from the contents of the other fields; if structure members are outside their valid interval, they will
       * be normalized (so that, for example, 40 October is changed into 9 November); tm_isdst is set (regardless of its
       * initial value) to a positive value or to 0, respectively, to indicate whether DST is or is not in effect at the
       * specified time.  Calling mktime() also sets the external variable tzname with information about the current timezone.
       */

#  ifndef TM_HAVE_TM_GMTOFF
      u_daylight = (tm.tm_isdst != 0);

      U_INTERNAL_PRINT("u_daylight = %d tzname[2] = { %s, %s }",
                        u_daylight,     tzname[0], tzname[1])
#  endif

      return true;
      }

   return false;
}

void u_init_ulib(char** restrict argv)
{
#ifndef _MSWINDOWS_
   const char* restrict pwd;
#endif

   U_INTERNAL_TRACE("u_init_ulib()")

   u_setPid();

   u_progpath = *argv;
   u_progname = u_basename(u_progpath);

   U_INTERNAL_ASSERT_POINTER(u_progname)

   u_progname_len = u__strlen(u_progname, __PRETTY_FUNCTION__);

   U_INTERNAL_ASSERT_MAJOR(u_progname_len, 0)

#ifdef USE_HARDWARE_CRC32
   __builtin_cpu_init();

   if (__builtin_cpu_supports ("sse4.2"))
      {
      uint32_t h = 0xABAD1DEA;

#  if __x86_64__
      h = (uint32_t)__builtin_ia32_crc32di(h, U_MULTICHAR_CONSTANT64('1','2','3','4','5','6','7','8'));
#  else
      h =           __builtin_ia32_crc32si(h, U_MULTICHAR_CONSTANT32('/','o','p','t'));
#  endif

      U_INTERNAL_ERROR(h, "hardware crc32 failed (h = %u). Exiting...", h);
      }
#endif

#ifdef _MSWINDOWS_
   u_init_ulib_mingw();
#endif

   u_getcwd(); /* get current working directory */

#ifndef _MSWINDOWS_
   pwd = getenv("PWD"); /* check for bash setting */

   if (pwd &&
       strncmp(u_cwd, pwd, u_cwd_len) != 0)
      {
#  ifdef DEBUG
      U_WARNING("current working directory from environment (PWD): %s differ from system getcwd(): %.*s", pwd, u_cwd_len, u_cwd);
#  endif
      }
#endif

   u_is_tty = isatty(STDERR_FILENO);

#ifdef HAVE_ATEXIT
   (void) atexit(u_exit); /* initialize AT EXIT */
#endif

   (void) u_setStartTime();

#ifdef DEBUG
   u_debug_init();
#endif
}

#ifdef ENTRY
#undef ENTRY
#endif
#define ENTRY(n,x) U_http_method_list[n].name =                 #x, \
                   U_http_method_list[n].len  = U_CONSTANT_SIZE(#x)

void u_init_http_method_list(void)
{
   U_INTERNAL_TRACE("u_init_http_method_list()")

   if (U_http_method_list[0].len == 0)
      {
      /* request methods */
      ENTRY(0,GET);
      ENTRY(1,HEAD);
      ENTRY(2,POST);
      ENTRY(3,PUT);
      ENTRY(4,DELETE);
      ENTRY(5,OPTIONS);
      /* pathological */
      ENTRY(6,TRACE);
      ENTRY(7,CONNECT);
      /* webdav */
      ENTRY(8,COPY);
      ENTRY(9,MOVE);
      ENTRY(10,LOCK);
      ENTRY(11,UNLOCK);
      ENTRY(12,MKCOL);
      ENTRY(13,SEARCH);
      ENTRY(14,PROPFIND);
      ENTRY(15,PROPPATCH);
      /* rfc-5789 */
      ENTRY(16,PATCH);
      ENTRY(17,PURGE);
      /* subversion */
      ENTRY(18,MERGE);
      ENTRY(19,REPORT);
      ENTRY(20,CHECKOUT);
      ENTRY(21,MKACTIVITY);
      /* upnp */
      ENTRY(22,NOTIFY);
      ENTRY(23,MSEARCH);
      ENTRY(24,SUBSCRIBE);
      ENTRY(25,UNSUBSCRIBE);
      }
}

#undef ENTRY

/**
 * Places characters into the array pointed to by s as controlled by the string
 * pointed to by format. If the total number of resulting characters including
 * the terminating null character is not more than maxsize, returns the number
 * of characters placed into the array pointed to by s (not including the
 * terminating null character); otherwise zero is returned and the contents of
 * the array indeterminate
 */

uint32_t u_strftime1(char* restrict s, uint32_t maxsize, const char* restrict format)
{
   static const char* dname[7]  = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
   static const char* mname[12] = { "January", "February", "March", "April", "May", "June", "July", "August", "September",
                                    "October", "November", "December" };

   static const int dname_len[7]  = { 6, 6, 7, 9, 8, 6, 8 };
   static const int mname_len[12] = { 7, 8, 5, 5, 3, 4, 4, 6, 9, 7, 8, 8 };

   /**
    * %% A single character %
    * %A The full name for the day of the week
    * %B The full name of the month
    * %H The hour (on a 24-hour clock), formatted with two digits
    * %I The hour (on a 12-hour clock), formatted with two digits
    * %M The minute, formatted with two digits
    * %S The second, formatted with two digits
    * %T The time in 24-hour notation (%H:%M:%S) (SU)
    * %U The week number, formatted with two digits (from 0 to 53; week number 1 is taken as beginning with the first Sunday in a year). See also %W
    * %W Another version of the week number: like %U, but counting week 1 as beginning with the first Monday in a year
    * %X A string representing the full time of day (hours, minutes, and seconds), in a format like 13:13:13
    * %Y The full year, formatted with four digits to include the century
    * %Z Defined by ANSI C as eliciting the time zone if available
    * %a An abbreviation for the day of the week
    * %b An abbreviation for the month name
    * %c A string representing the complete date and time, in the form Mon Apr 01 13:13:13 1992
    * %d The day of the month, formatted with two digits
    * %e Like %d, the day of the month as a decimal number, but a leading zero is replaced by a space
    * %h Equivalent to %b (SU)
    * %j The count of days in the year, formatted with three digits (from 1 to 366)
    * %m The month number, formatted with two digits
    * %p Either AM or PM as appropriate
    * %w A single digit representing the day of the week: Sunday is day 0
    * %x A string representing the complete date, in a format like Mon Apr 01 1992
    * %y The last two digits of the year
    * %z The +hhmm or -hhmm numeric timezone (that is, the hour and minute offset from UTC)
    */

   static const int dispatch_table[] = {
      (char*)&&case_A-(char*)&&cdefault,/* 'A' */
      (char*)&&case_B-(char*)&&cdefault,/* 'B' */
      0,/* 'C' */
      0,/* 'D' */
      0,/* 'E' */
      0,/* 'F' */
      0,/* 'G' */
      (char*)&&case_H-(char*)&&cdefault,/* 'H' */
      (char*)&&case_I-(char*)&&cdefault,/* 'I' */
      0,/* 'J' */
      0,/* 'K' */
      0,/* 'L' */
      (char*)&&case_M-(char*)&&cdefault,/* 'M' */
      0,/* 'N' */
      0,/* 'O' */
      0,/* 'P' */
      0,/* 'Q' */
      0,/* 'R' */
      (char*)&&case_S-(char*)&&cdefault,/* 'S' */
      (char*)&&case_T-(char*)&&cdefault,/* 'T' */
      (char*)&&case_U-(char*)&&cdefault,/* 'U' */
      0,/* 'V' */
      (char*)&&case_W-(char*)&&cdefault,/* 'W' */
      (char*)&&case_T-(char*)&&cdefault,/* 'X' */
      (char*)&&case_Y-(char*)&&cdefault,/* 'Y' */
      (char*)&&case_Z-(char*)&&cdefault,/* 'Z' */
      0,/* '[' */
      0,/* '\' */
      0,/* ']' */
      0,/* '^' */
      0,/* '_' */
      0,/* '`' */
      (char*)&&case_a-(char*)&&cdefault,/* 'a' */
      (char*)&&case_b-(char*)&&cdefault,/* 'b' */
      (char*)&&case_c-(char*)&&cdefault,/* 'c' */
      (char*)&&case_d-(char*)&&cdefault,/* 'd' */
      (char*)&&case_e-(char*)&&cdefault,/* 'e' */
      0,/* 'f' */
      0,/* 'g' */
      (char*)&&case_b-(char*)&&cdefault,/* 'h' */
      0,/* 'i' */
      (char*)&&case_j-(char*)&&cdefault,/* 'j' */
      0,/* 'k' */
      0,/* 'l' */
      (char*)&&case_m-(char*)&&cdefault,/* 'm' */
      0,/* 'n' */
      0,/* 'o' */
      (char*)&&case_p-(char*)&&cdefault,/* 'p' */
      0,/* 'q' */
      0,/* 'r' */
      0,/* 's' */
      0,/* 't' */
      0,/* 'u' */
      0,/* 'v' */
      (char*)&&case_w-(char*)&&cdefault,/* 'w' */
      (char*)&&case_x-(char*)&&cdefault,/* 'x' */
      (char*)&&case_y-(char*)&&cdefault,/* 'y' */
      (char*)&&case_z-(char*)&&cdefault /* 'z' */
   };

   char ch;                      /* character from format */
   int i, n, val;                /* handy integer (short term usage) */
   uint32_t count = 0;           /* return value accumulator */
   const char* restrict fmark;   /* for remembering a place in format */

   U_INTERNAL_TRACE("u_strftime1(%u,%s)", maxsize, format)

   U_INTERNAL_ASSERT_POINTER(format)
   U_INTERNAL_ASSERT_MAJOR(maxsize, 0)

   while (count < maxsize)
      {
      fmark = format;

      while ((ch = *format) != '\0')
         {
         if (ch == '%') break;

         ++format;
         }

      if ((n = (format - fmark)) != 0)
         {
         while (n--) s[count++] = *fmark++;
         }

      if (ch == '\0') break;

      ++format; /* skip over '%' */

      ch = *format++;

      if (u__isalpha(ch) == false)
         {
cdefault:
         s[count++] = '%'; /* "%%" prints % */

         if (ch != '%') /* "%?" prints %?, unless ? is 0: pretend it was %c with argument ch */
            {
            if (ch == '\0') break;

            s[count++] = ch;
            }

         continue;
         }

      U_INTERNAL_PRINT("dispatch_table[%d] = %p &&cdefault = %p", ch-'A', dispatch_table[ch-'A'], &&cdefault)

      goto *((char*)&&cdefault + dispatch_table[ch-'A']);

case_A: /* %A The full name for the day of the week */
      for (i = 0; i < dname_len[u_strftime_tm.tm_wday]; ++i) s[count++] = dname[u_strftime_tm.tm_wday][i];

      continue;

case_B: /* %B The full name of the month */
      for (i = 0; i < mname_len[u_strftime_tm.tm_mon]; ++i) s[count++] = mname[u_strftime_tm.tm_mon][i];

      continue;

case_H: /* %H The hour (on a 24-hour clock), formatted with two digits */
      U_INTERNAL_ASSERT(count <= (maxsize-2))

   /* if (count >= (maxsize - 2)) return 0; */

      U_NUM2STR16(s+count, u_strftime_tm.tm_hour);

      count += 2;

      continue;

case_I: /* %I The hour (on a 12-hour clock), formatted with two digits */
      U_INTERNAL_ASSERT(count <= (maxsize-2))

   /* if (count >= (maxsize - 2)) return 0; */

      if (u_strftime_tm.tm_hour == 0 ||
          u_strftime_tm.tm_hour == 12)
         {
         u_put_unalignedp16(s+count, U_MULTICHAR_CONSTANT16('1', '2'));
         }
      else
         {
         val = u_strftime_tm.tm_hour % 12;

         u_put_unalignedp16(s+count, U_MULTICHAR_CONSTANT16('0' + (val >= 10 ? (val / 10) : 0),
                                                            '0' + (val  % 10)));
         }

      count += 2;

      continue;

case_M: /* %M The minute, formatted with two digits */
      U_INTERNAL_ASSERT(count <= (maxsize-2))

   /* if (count >= (maxsize - 2)) return 0; */

      U_NUM2STR16(s+count, u_strftime_tm.tm_min);

      count += 2;

      continue;

case_S: /* %S The second, formatted with two digits */
      U_INTERNAL_ASSERT(count <= (maxsize-2))

   /* if (count >= (maxsize - 2)) return 0; */

      U_NUM2STR16(s+count, u_strftime_tm.tm_sec);

      count += 2;

      continue;

case_T: /* %X A string representing the full time of day (hours, minutes, and seconds), in a format like 13:13:13 - %T The time in 24-hour notation (%H:%M:%S) (SU) */
      U_INTERNAL_ASSERT(count <= (maxsize-8))

   /* if (count >= (maxsize - 8)) return 0; */

      U_NUM2STR64(s+count,':',u_strftime_tm.tm_hour,u_strftime_tm.tm_min,u_strftime_tm.tm_sec);

      count += 8;

      continue;

case_U: /* %U The week number, formatted with two digits (from 0 to 53; week number 1 is taken as beginning with the first Sunday in a year). See also %W */
      U_INTERNAL_ASSERT(count <= (maxsize-2))

   /* if (count >= (maxsize - 2)) return 0; */

      U_NUM2STR16(s+count, (u_strftime_tm.tm_yday + 7 - u_strftime_tm.tm_wday) / 7);

      count += 2;

      continue;

case_W: /* %W Another version of the week number: like %U, but counting week 1 as beginning with the first Monday in a year */
      U_INTERNAL_ASSERT(count <= (maxsize-2))

   /* if (count >= (maxsize - 2)) return 0; */

      U_NUM2STR16(s+count, (u_strftime_tm.tm_yday + ((8-u_strftime_tm.tm_wday) % 7)) / 7);

      count += 2;

      continue;

case_Y: /* %Y The full year, formatted with four digits to include the century */
      U_INTERNAL_ASSERT(count <= (maxsize-4))

   /* if (count >= (maxsize - 4)) return 0; */

      (void) sprintf(s+count, "%.4d", 1900 + u_strftime_tm.tm_year);

      count += 4;

      continue;

case_Z: /* %Z Defined by ANSI C as eliciting the time zone if available */
      n = u__strlen(tzname[u_daylight], __PRETTY_FUNCTION__);

      U_INTERNAL_ASSERT(count <= (maxsize-n))

   /* if (count >= (maxsize - n)) return 0; */

      (void) u__strncpy(s+count, tzname[u_daylight], n);

      count += n;

      continue;

case_a: /* %a An abbreviation for the day of the week */
      for (i = 0; i < 3; ++i) s[count++] = dname[u_strftime_tm.tm_wday][i];

      continue;

case_b: /* %b An abbreviation for the month name - %h Equivalent to %b (SU) */
      for (i = 0; i < 3; ++i) s[count++] = mname[u_strftime_tm.tm_mon][i];

      continue;

case_c: /* %c A string representing the complete date and time, in the form Mon Apr 01 13:13:13 1992 */
      U_INTERNAL_ASSERT(count <= (maxsize-24))

   // if (count >= (maxsize - 24)) return 0;

      for (i = 0; i < 3; ++i) s[count++] = dname[u_strftime_tm.tm_wday][i];
                              s[count++] = ' ';
      for (i = 0; i < 3; ++i) s[count++] = mname[u_strftime_tm.tm_mon][i];

      (void) sprintf(&s[count], " %.2d %2.2d:%2.2d:%2.2d %.4d", u_strftime_tm.tm_mday, u_strftime_tm.tm_hour,
                                                                u_strftime_tm.tm_min,  u_strftime_tm.tm_sec,
                                                                1900 + u_strftime_tm.tm_year);

      count += 17;

      continue;

case_d: /* %d The day of the month, formatted with two digits */
      U_INTERNAL_ASSERT(count <= (maxsize-2))

   /* if (count >= (maxsize - 2)) return 0; */

      U_NUM2STR16(s+count, u_strftime_tm.tm_mday);

      count += 2;

      continue;

case_e: /* %e Like %d, the day of the month as a decimal number, but a leading zero is replaced by a space */
      U_INTERNAL_ASSERT(count <= (maxsize-2))

   /* if (count >= (maxsize - 2)) return 0; */

      val = (u_strftime_tm.tm_mday >= 10 ? (u_strftime_tm.tm_mday / 10) : 0);

      u_put_unalignedp16(s+count, U_MULTICHAR_CONSTANT16(val ? '0' + val : ' ',
                                                               '0' + (u_strftime_tm.tm_mday % 10)));

      count += 2;

      continue;

case_j: /* %j The count of days in the year, formatted with three digits (from 1 to 366) */
      U_INTERNAL_ASSERT(count <= (maxsize-3))

   /* if (count >= (maxsize - 3)) return 0; */

      (void) sprintf(s+count, "%.3d", u_strftime_tm.tm_yday+1);

      count += 3;

      continue;

case_m: /* %m The month number, formatted with two digits */
      U_INTERNAL_ASSERT(count <= (maxsize-2))

   /* if (count >= (maxsize - 2)) return 0; */

      U_NUM2STR16(s+count, u_strftime_tm.tm_mon+1);

      count += 2;

      continue;

case_p: /* %p Either AM or PM as appropriate */
      U_INTERNAL_ASSERT(count <= (maxsize-2))

   /* if (count >= (maxsize - 2)) return 0; */

      u_put_unalignedp16(s+count, U_MULTICHAR_CONSTANT16(u_strftime_tm.tm_hour < 12 ? 'A' : 'P','M'));

      count += 2;

      continue;

case_w: /* %w A single digit representing the day of the week: Sunday is day 0 */
      U_INTERNAL_ASSERT(count <= (maxsize-1))

   /* if (count >= (maxsize - 1)) return 0; */

      s[count++] = '0' + (u_strftime_tm.tm_wday % 10);

      continue;

case_x: /* %x A string representing the complete date, in a format like Mon Apr 01 1992 */
      U_INTERNAL_ASSERT(count <= (maxsize-15))

   /* if (count >= (maxsize - 15)) return 0; */

      for (i = 0; i < 3; i++) s[count++] = dname[u_strftime_tm.tm_wday][i];
                              s[count++] = ' ';
      for (i = 0; i < 3; i++) s[count++] = mname[u_strftime_tm.tm_mon][i];

      (void) sprintf(&s[count], " %.2d %.4d", u_strftime_tm.tm_mday, 1900 + u_strftime_tm.tm_year);

      count += 8;

      continue;

case_y: /* %y The last two digits of the year */
      U_INTERNAL_ASSERT(count <= (maxsize-2))

   /* if (count >= (maxsize - 2)) return 0; */

      /**
       * The year could be greater than 100, so we need the value modulo 100.
       * The year could be negative, so we need to correct for a possible negative remainder.
       */

      U_NUM2STR16(s+count, ((u_strftime_tm.tm_year % 100) + 100) % 100);

      count += 2;

      continue;

case_z: /* %z The +hhmm or -hhmm numeric timezone (that is, the hour and minute offset from UTC) */
      U_INTERNAL_ASSERT(count <= (maxsize-5))

   /* if (count >= (maxsize - 5)) return 0; */

      val = (u_now_adjust / 3600);

      if (val > 0)
         {
         s[count++] = '+';

         U_NUM2STR16(s+count, val);
         }
      else
         {
         s[count++] = '-';

         U_NUM2STR16(s+count, -val);
         }

      U_NUM2STR16(s+count+2, u_now_adjust % 3600);

      count += 4;
      }

   U_INTERNAL_PRINT("count = %u maxsize = %u", count, maxsize)

   if (count < maxsize) s[count] = '\0';

   U_INTERNAL_ASSERT(count <= maxsize)

   return count;
}

uint32_t u_strftime2(char* restrict s, uint32_t maxsize, const char* restrict format, time_t t)
{
   U_INTERNAL_TRACE("u_strftime2(%u,%s,%ld)", maxsize, format, t)

   U_INTERNAL_ASSERT_POINTER(format)
   U_INTERNAL_ASSERT_MAJOR(maxsize, 0)

   (void) memset(&u_strftime_tm, 0, sizeof(struct tm));

   (void) gmtime_r(&t, &u_strftime_tm);

   return u_strftime1(s, maxsize, format);
}

uint32_t u_num2str32(char* restrict cp, uint32_t num)
{
   int32_t i = 0;
   uint32_t ui32vec[4];
   char* restrict start = cp;

   U_INTERNAL_TRACE("u_num2str32(%p,%u)", cp, num)

   while (num >= 100U)
      {
      ui32vec[i++] = (num  % 100U);
                      num /= 100U;
      }

   /* Handle last 1-2 digits */

   if (num < 10U) *cp++ = '0' + num;
   else
      {
      U_NUM2STR16(cp, num);

      cp += 2;
      }

   while (--i >= 0)
      {
      U_NUM2STR16(cp, ui32vec[i]);

      cp += 2;
      }

   return (cp - start);
}

uint32_t u_num2str32s(char* restrict cp, int32_t num)
{
   U_INTERNAL_TRACE("u_num2str32s(%p,%d)", cp, num)

   if (num < 0)
      {
      num = -num;

      *cp++ = '-';
      }

   return u_num2str32(cp, num);
}

uint32_t u_num2str64(char* restrict cp, uint64_t num)
{
   int32_t i;
   uint64_t ui64vec[10];
   char* restrict start;

   U_INTERNAL_TRACE("u_num2str64(%p,%llu)", cp, num)

   /**
    * To divide 64-bit numbers and to find remainders on the x86 platform gcc and icc call the libc functions
    * [u]divdi3() and [u]moddi3(), and they call another function in its turn. On FreeBSD it is the qdivrem()
    * function, its source code is about 170 lines of the code. The glibc counterpart is about 150 lines of code.
    *
    * For 32-bit numbers and some divisors gcc and icc use a inlined multiplication and shifts.
    * For example, unsigned "i32 / 10" is compiled to (i32 * 0xCCCCCCCD) >> 35
    */

   if (num <= UINT_MAX) return u_num2str32(cp, (uint32_t)num);

   i     = 0;
   start = cp;

   /* Maximum value an `unsigned long long int' can hold: 18446744073709551615ULL */

   while (num >= 100ULL)
      {
      ui64vec[i++] = (num  % 100ULL);
                      num /= 100ULL;
      }

   /* Handle last 1-2 digits */

   if (num < 10ULL) *cp++ = '0' + num;
   else
      {
      U_NUM2STR16(cp, num);

      cp += 2;
      }

   while (--i >= 0)
      {
      U_NUM2STR16(cp, ui64vec[i]);

      cp += 2;
      }

   return (cp - start);
}

uint32_t u_num2str64s(char* restrict cp, int64_t num)
{
   U_INTERNAL_TRACE("u_num2str64s(%p,%lld)", cp, num)

   if (num < 0LL)
      {
      num = -num;

      *cp++ = '-';
      }

   return u_num2str64(cp, num);
}

#ifdef DEBUG
#  include <ulib/base/trace.h>

static bool u_askForContinue(void)
{
   U_INTERNAL_TRACE("u_askForContinue()")

   if (u_is_tty &&
       isatty(STDIN_FILENO))
      {
      char ch[2];

      // NB: we use U_MESSAGE here, but we are already inside u__printf()...

      int u_flag_exit_save = u_flag_exit;
                             u_flag_exit = 0;

      U_MESSAGE("Press '%Wc%W' to continue, '%W%s%W' to exit: %W", GREEN, YELLOW, RED, "Enter", YELLOW, RESET);

      u_flag_exit = u_flag_exit_save;

      if (read(STDIN_FILENO, ch, 1) == 1 &&
          ch[0] == 'c'                   &&
          read(STDIN_FILENO, ch, 1) == 1) /* get 'return' key */
         { 
         return true;
         }
      }

   return false;
}
#endif

void u_internal_print(bool abrt, const char* restrict format, ...)
{
   uint32_t bytes_written;
   char u_internal_buf[16 * 1024];

   va_list argp;
   va_start(argp, format);

   (void) vsnprintf(u_internal_buf, sizeof(u_internal_buf), format, argp);

   va_end(argp);

#ifdef DEBUG
   if (abrt) u_printError();
#endif

   bytes_written = strlen(u_internal_buf);

   (void) write(STDERR_FILENO, u_internal_buf, bytes_written);

   if (abrt)
      {
#ifdef DEBUG
      /* NB: registra l'errore sul file di trace, check stderr per evitare duplicazione messaggio a video */

      if (u_trace_fd > STDERR_FILENO)
         {
         struct iovec iov[1] = { { (caddr_t)u_internal_buf, bytes_written } };

         u_trace_writev(iov, 1);
         }

      if (u_askForContinue() == false)
#endif
         {
         u_flag_exit = -2; // abort...

         u_debug_at_exit();
         }
      }
}

/**
 * --------------------------------------------------------------------
 * Encode escape sequences into a buffer, the following are recognized:
 * --------------------------------------------------------------------
 *  \a  BEL                 (\007  7  7)
 *  \b  BS  backspace       (\010  8  8) 
 *  \t  HT  horizontal tab  (\011  9  9)
 *  \n  LF  newline         (\012 10  A) 
 *  \v  VT  vertical tab    (\013 11  B)
 *  \f  FF  formfeed        (\014 12  C) 
 *  \r  CR  carriage return (\015 13  D)
 *  \e  ESC character       (\033 27 1B)
 *
 *  \DDD number formed of 1-3 octal digits
 * --------------------------------------------------------------------
 */

uint32_t u_sprintc(char* restrict out, unsigned char c)
{
   char* restrict cp;

   U_INTERNAL_TRACE("u_sprintc(%d)", c)

   if (c < 32)
      {
      *out++ = '\\';

      switch (c)
         {
         case '\a': // 0x07
            {
            *out = 'a';

            return 2;
            }

         case '\b': // 0x08
            {
            *out = 'b';

            return 2;
            }

         case '\t': // 0x09
            {
            *out = 't';

            return 2;
            }

         case '\n': // 0x0A
            {
            *out = 'n';

            return 2;
            }

         case '\v': // 0x0B
            {
            *out = 'v';

            return 2;
            }

         case '\f': // 0x0C
            {
            *out = 'f';

            return 2;
            }

         case '\r': // 0x0D
            {
            *out = 'r';

            return 2;
            }

         case '\033': // 0x1B
            {
            *out = 'e';

            return 2;
            }

         default: goto next;
         }
      }

   if (c == '"' || // 0x22
       c == '\\')  // 0x5C
      {
      *out++ = '\\';
      *out   = c;

      return 2;
      }

   if (c > 126)
      {
      *out++ = '\\';

      /* \DDD number formed of 1-3 octal digits */
next:
      cp = out + 3;

      do {
         *--cp = (c & 7) + '0';

         c >>= 3;
         }
      while (c);

      while (--cp >= out) *cp = '0';

      return 4;
      }

   *out = c;

   return 1;
}

#if !defined(_MSWINDOWS_) && !defined(__UNIKERNEL__)
static const char* tab_color[] = { U_RESET_STR,
   U_BLACK_STR,       U_RED_STR,           U_GREEN_STR,       U_YELLOW_STR,
   U_BLUE_STR,        U_MAGENTA_STR,       U_CYAN_STR,        U_WHITE_STR,
   U_BRIGHTBLACK_STR, U_BRIGHTRED_STR,     U_BRIGHTGREEN_STR, U_BRIGHTYELLOW_STR,
   U_BRIGHTBLUE_STR,  U_BRIGHTMAGENTA_STR, U_BRIGHTCYAN_STR,  U_BRIGHTWHITE_STR };
#endif

/**
 * ----------------------------------------------------------------------------
 * Print with format extension: bBCDHMNOPQrRSUvVYwW
 * ----------------------------------------------------------------------------
 * '%b': print bool ("true" or "false")
 * '%B': print bit conversion of integer
 * '%C': print formatted char
 * '%H': print name host
 * '%M': print memory dump
 * '%N': print name program
 * '%P': print pid process
 * '%Q': sign for call to exit() or abort() (var-argument is param to exit)
 * '%r': print u_getExitStatus(exit_value)
 * '%R': print var-argument (msg) "-" u_getSysError()
 * '%O': print formatted temporary string + free(string)
 * '%S': print formatted string
 * '%v': print ustring
 * '%V': print ustring
 * '%J': print U_DATA
 * '%U': print name login user
 * '%Y': print u_getSysSignal(signo)
 * '%w': print current working directory
 * '%W': print COLOR (index to ANSI ESCAPE STR)
 * ----------------------------------------------------------------------------
 * '%D': print date and time in various format:
 * ----------------------------------------------------------------------------
 *             0  => format: %d/%m/%y
 * with flag  '1' => format:          %T (=> "%H:%M:%S)
 * with flag  '2' => format:          %T (=> "%H:%M:%S) +n days
 * with flag  '3' => format: %d/%m/%Y %T
 * with flag  '4' => format: %d%m%y_%H%M%S_millisec (for file name, backup, etc...)
 * with flag  '5' => format: %a, %d %b %Y %T %Z
 * with flag  '6' => format: %Y/%m/%d
 * with flag  '7' => format: %Y/%m/%d %T
 * with flag  '8' => format: %a, %d %b %Y %T GMT
 * with flag  '9' => format: %d/%m/%y %T
 * with flag '10' => format: %d/%b/%Y:%T %z
 * with flag  '#' => var-argument
 * ----------------------------------------------------------------------------
 */

uint32_t u__vsnprintf(char* restrict buffer, uint32_t buffer_size, const char* restrict format, va_list argp)
{
   static const int dispatch_table[] = {
      (char*)&&case_space-(char*)&&cdefault, /* ' ' */
      0,/* '!' */
      0,/* '"' */
      (char*)&&case_number-(char*)&&cdefault,/* '#' */
      0,/* '$' */
      0,/* '%' */
      0,/* '&' */
      (char*)&&case_quote-(char*)&&cdefault, /* '\'' */
      0,/* '(' */
      0,/* ')' */
      (char*)&&case_asterisk-(char*)&&cdefault,/* '*' */
      (char*)&&case_plus-(char*)&&cdefault,    /* '+' */
      0,/* ',' */
      (char*)&&case_minus-(char*)&&cdefault, /* '-' */
      (char*)&&case_period-(char*)&&cdefault,/* '.' */
      0,/* '/' */
      (char*)&&case_zero-(char*)&&cdefault,  /* '0' */
      (char*)&&case_digit-(char*)&&cdefault, /* '1' */
      (char*)&&case_digit-(char*)&&cdefault, /* '2' */
      (char*)&&case_digit-(char*)&&cdefault, /* '3' */
      (char*)&&case_digit-(char*)&&cdefault, /* '4' */
      (char*)&&case_digit-(char*)&&cdefault, /* '5' */
      (char*)&&case_digit-(char*)&&cdefault, /* '6' */
      (char*)&&case_digit-(char*)&&cdefault, /* '7' */
      (char*)&&case_digit-(char*)&&cdefault, /* '8' */
      (char*)&&case_digit-(char*)&&cdefault, /* '9' */
      0,/* ':' */
      0,/* ';' */
      0,/* '<' */
      0,/* '=' */
      0,/* '>' */
      0,/* '?' */
      0,/* '@' */
      (char*)&&case_float-(char*)&&cdefault,/* 'A' */
      (char*)&&case_B-(char*)&&cdefault,/* 'B' */
      (char*)&&case_C-(char*)&&cdefault,/* 'C' */
      (char*)&&case_D-(char*)&&cdefault,/* 'D' */
      (char*)&&case_float-(char*)&&cdefault,/* 'E' */
      (char*)&&case_float-(char*)&&cdefault,/* 'F' */
      (char*)&&case_float-(char*)&&cdefault,/* 'G' */
      (char*)&&case_H-(char*)&&cdefault,/* 'H' */
      (char*)&&case_I-(char*)&&cdefault,/* 'I' */
      (char*)&&case_J-(char*)&&cdefault,/* 'J' */
      0,/* 'K' */
      (char*)&&case_L-(char*)&&cdefault,/* 'L' */
      (char*)&&case_M-(char*)&&cdefault,/* 'M' */
      (char*)&&case_N-(char*)&&cdefault,/* 'N' */
      (char*)&&case_str-(char*)&&cdefault,/* 'O' */
      (char*)&&case_P-(char*)&&cdefault,/* 'P' */
      (char*)&&case_Q-(char*)&&cdefault,/* 'Q' */
      (char*)&&case_R-(char*)&&cdefault,/* 'R' */
      (char*)&&case_str-(char*)&&cdefault,/* 'S' */
      (char*)&&case_T-(char*)&&cdefault,/* 'T' */
      (char*)&&case_U-(char*)&&cdefault,/* 'U' */
      (char*)&&case_V-(char*)&&cdefault,/* 'V' */
      (char*)&&case_W-(char*)&&cdefault,/* 'W' */
      (char*)&&case_X-(char*)&&cdefault,/* 'X' */
      (char*)&&case_Y-(char*)&&cdefault,/* 'Y' */
      0,/* 'Z' */
      0,/* '[' */
      0,/* '\' */
      0,/* ']' */
      0,/* '^' */
      0,/* '_' */
      0,/* '`' */
      (char*)&&case_float-(char*)&&cdefault,/* 'a' */
      (char*)&&case_b-(char*)&&cdefault,/* 'b' */
      (char*)&&case_c-(char*)&&cdefault,/* 'c' */
      (char*)&&case_d-(char*)&&cdefault,/* 'd' */
      (char*)&&case_float-(char*)&&cdefault,/* 'e' */
      (char*)&&case_float-(char*)&&cdefault,/* 'f' */
      (char*)&&case_float-(char*)&&cdefault,/* 'g' */
      (char*)&&case_h-(char*)&&cdefault,/* 'h' */
      (char*)&&case_d-(char*)&&cdefault,/* 'i' */
      (char*)&&case_j-(char*)&&cdefault,/* 'j' */
      0,/* 'k' */
      (char*)&&case_l-(char*)&&cdefault,/* 'l' */
      0,/* 'm' */
      0,/* 'n' */
      (char*)&&case_o-(char*)&&cdefault,/* 'o' */
      (char*)&&case_p-(char*)&&cdefault,/* 'p' */
      (char*)&&case_q-(char*)&&cdefault,/* 'q' */
      (char*)&&case_r-(char*)&&cdefault,/* 'r' */
      (char*)&&case_str-(char*)&&cdefault,/* 's' */
      0,/* 't' */
      (char*)&&case_u-(char*)&&cdefault,/* 'u' */
      (char*)&&case_v-(char*)&&cdefault,/* 'v' */
      (char*)&&case_w-(char*)&&cdefault,/* 'w' */
      (char*)&&case_X-(char*)&&cdefault,/* 'x' */
      0,/* 'y' */
      0 /* 'z' */
   };

   int pads;     /* extra padding size */
   int dpad;     /* extra 0 padding needed for integers */
   int bpad;     /* extra blank padding needed */
   int size;     /* size of converted field or string */
   int width;    /* width from format (%8d), or 0 */
   int prec;     /* precision from format (%.3d), or -1 */
   int dprec;    /* a copy of prec if [diouxX], 0 otherwise */
   int fieldsz;  /* field size expanded by sign, dpad etc */

   char sign;                      /* sign prefix (' ', '+', '-', or \0) */
   const char* restrict fmark;     /* for remembering a place in format */
   unsigned char buf_number[32];   /* space for %[cdiouxX] */
   unsigned char* restrict cp = 0; /* handy char pointer (short term usage) */

   uint64_t argument = 0;          /* integer arguments %[diIouxX] */
   enum { OCT, DEC, HEX } base;    /* base for [diIouxX] conversion */

   int flags; /* flags as above */

   /* Flags used during conversion */

#  define LONGINT            0x001 /* long integer */
#  define LLONGINT           0x002 /* long long integer */
#  define LONGDBL            0x004 /* long double */
#  define SHORTINT           0x008 /* short integer */
#  define ALT                0x010 /* alternate form */
#  define LADJUST            0x020 /* left adjustment */
#  define ZEROPAD            0x040 /* zero (as opposed to blank) */
#  define HEXPREFIX          0x080 /* add 0x or 0X prefix */
#  define THOUSANDS_GROUPED  0x100 /* For decimal conversion (i,d,u,f,F,g,G) the output is to be grouped with thousands */

   /* To extend shorts properly, we need both signed and uint32_t argument extraction methods */

#  define VA_ARG(type) va_arg(argp, type)

#  define SARG() (flags & LLONGINT ?                      VA_ARG(int64_t) : \
                  flags &  LONGINT ? (int64_t)            VA_ARG(long) : \
                  flags & SHORTINT ? (int64_t)(int16_t)   VA_ARG(int)  : \
                                     (int64_t)            VA_ARG(int))
#  define UARG() (flags & LLONGINT ?                      VA_ARG(uint64_t) : \
                  flags &  LONGINT ? (uint64_t)           VA_ARG(unsigned long) : \
                  flags & SHORTINT ? (uint64_t)(uint16_t) VA_ARG(int) : \
                                     (uint64_t)           VA_ARG(unsigned int))

   /* Scan the format for conversions (`%' character) */

   char ch;          /* character from format */
   time_t t;
   uint32_t ret = 0; /* return value accumulator */
   unsigned char c;
   struct U_DATA udata;
   char* restrict pbase;
   struct ustringrep* pstr;
   const char* restrict ccp;
   char* restrict bp = buffer;
   unsigned char fmt_float[32];
   const char* restrict fmtdate;
   int i, n, len, maxlen, remaining; /* handy integer (short term usage) */
#ifdef DEBUG
   const char* restrict format_save = format;
#endif

   U_INTERNAL_TRACE("u__vsnprintf(%p,%u,%s)", buffer, buffer_size, format)

   U_INTERNAL_ERROR(buffer_size, "ZERO BUFFER SIZE at u__vsnprintf()", 0);

   while (true)
      {
      U_INTERNAL_ERROR(ret <= buffer_size, "BUFFER OVERFLOW at u__vsnprintf() ret = %u buffer_size = %u format = \"%s\"", ret, buffer_size, format_save);

      fmark = format;

      while ((ch = *format) != '\0')
         {
         if (ch == '%') break;

         ++format;
         }

      if ((n = (format - fmark)) != 0)
         {
         ret += n;

         while (n--) *bp++ = *fmark++;
         }

      if (ch == '\0') break;

      prec  = -1;
      sign  = '\0';
      width = flags = dprec = 0;

      ++format; /* skip over '%' */

rflag:
      ch = *format++;

reswitch:
      U_INTERNAL_PRINT("prec = %d ch = %c", prec, ch)

      if (UNLIKELY(u__isprintf(ch) == false))
         {
cdefault:
         *bp++ = '%'; /* "%%" prints % */

         ++ret;

         if (ch != '%') /* "%?" prints %?, unless ? is 0: pretend it was %c with argument ch */
            {
            if (ch == '\0') break;

            *bp++ = ch;

            ++ret;
            }

         continue;
         }

      U_INTERNAL_PRINT("dispatch_table[%d] = %p &&cdefault = %p", ch-'A', dispatch_table[ch-' '], &&cdefault)

      goto *((char*)&&cdefault + dispatch_table[ch-' ']);

case_space: /* If the space and + flags both appear, the space flag will be ignored */
      if (!sign) sign = ' ';

      goto rflag;

case_number: /* field flag characters: # */
      flags |= ALT;

      goto rflag;

case_quote: /* For decimal conversion (i,d,u,f,F,g,G) the output is to be grouped with thousands */
      flags |= THOUSANDS_GROUPED;

      goto rflag;

case_asterisk: /* A negative field width argument is taken as a - flag followed by a positive field width. They don't exclude field widths read from args */
      if ((width = VA_ARG(int)) >= 0) goto rflag;

      width = -width;

case_minus: /* field flag characters: - */
      flags |= LADJUST;
      flags &= ~ZEROPAD; /* '-' disables '0' */

      goto rflag;

case_plus: /* field flag characters: + */
      sign = '+';

      goto rflag;

case_period: /* The field precision '.' */
      if ((ch = *format++) == '*')
         {
         prec = VA_ARG(int);

         ch = *format++;

         U_INTERNAL_PRINT("prec = %d ch = %c", prec, ch)

         if (u__tolower(ch) == 's') goto case_str;
         }
      else
         {
         for (prec = 0; u__isdigit(ch); ch = *format++) prec = (prec*10) + (ch-'0');
         }

      goto reswitch;

case_zero: /* Note that 0 is taken as a flag, not as the beginning of a field width */
      if (!(flags & LADJUST)) flags |= ZEROPAD; /* '-' disables '0' */

      goto rflag;

case_digit: /* field width: [1-9] - An optional decimal digit string (with nonzero first digit) specifying a minimum field width */
      n = 0;

      do {
         n  = n*10 + (ch-'0');
         ch = *format++;
         }
      while (u__isdigit(ch));

      width = n;

      goto reswitch;

case_float:
      fmt_float[0] = '%';

      cp = fmt_float + 1;

      if (flags & ALT)               *cp++ = '#';
      if (flags & ZEROPAD)           *cp++ = '0';
      if (flags & LADJUST)           *cp++ = '-';
      if (flags & THOUSANDS_GROUPED) *cp++ = '\'';
      if (sign != '\0')              *cp++ = sign;

      *cp++ = '*'; /* width */
      *cp++ = '.';
      *cp++ = '*'; /* prec */

      if (flags & LONGDBL) *cp++ = 'L';

      *cp++ = ch;
      *cp   = '\0';

      if (flags & LONGDBL)
         {
         long double ldbl = VA_ARG(long double);

         (void) sprintf(bp, (const char* restrict)fmt_float, width, prec, ldbl);
         }
      else
         {
         double dbl = VA_ARG(double);

         (void) sprintf(bp, (const char* restrict)fmt_float, width, prec, dbl);
         }

      len = u__strlen(bp, __PRETTY_FUNCTION__);

      bp  += len;
      ret += len;

      continue;

case_str:
      /* s: print                     string */
      /* S: print formatted           string */
      /* O: print formatted temporary string + plus free(string) */

      cp = VA_ARG(unsigned char* restrict);

      if (cp == 0)
         {
         U_INTERNAL_PRINT("prec = %d", prec)

         if (prec == 0)
            {
            U_INTERNAL_PRINT("ch = %c", ch)

            if (ch == 'S') goto empty;
            }
         else if (prec == -1)
            {
            u_put_unalignedp32(bp,   U_MULTICHAR_CONSTANT32('(','n','u','l'));
            u_put_unalignedp16(bp+4, U_MULTICHAR_CONSTANT16('l',')'));

            bp  += 6;
            ret += 6;
            }

         continue;
         }

      if (ch != 's')
         {
case_ustring_V:
         U_INTERNAL_PRINT("prec = %d u_printf_string_max_length = %d", prec, u_printf_string_max_length)

         if (prec == 0)
            {
empty:      u_put_unalignedp16(bp, U_MULTICHAR_CONSTANT16('"','"'));

            bp  += 2;
            ret += 2;

            continue;
            }

         len = prec;

         maxlen = (u_printf_string_max_length > 0
                        ? u_printf_string_max_length
                        : 128);

         if (prec < 0 || /* NB: no precision specified... */
             prec > maxlen)
            {
            prec = maxlen;
            }

         remaining = buffer_size - ret;

         if ((flags & ALT) == 0) /* NB: # -> force print of all binary string (compatibly with buffer size)... */
            {
            remaining -= (prec * 4); /* worst case is \DDD number formed of 1-3 octal digits */
            }

         n     = 0;
         pbase = bp;
         *bp++ = '"';

         while (true)
            {
            if (cp[n] == '\0' &&
                (flags & ALT) == 0)
               {
               break;
               }

            i = u_sprintc(bp, cp[n]);

            bp        += i;
            remaining -= i;

            if (++n >= prec ||
                remaining <= 60)
               {
               break;
               }
            }

         *bp++ = '"';

         if (cp[n]   && /* NB: no null terminator... */
             len < 0 && /* NB: no precision specified... */
             n == prec)
            {
            /* to be continued... */

                              *bp++ = '.';
            u_put_unalignedp16(bp, U_MULTICHAR_CONSTANT16('.','.'));

            bp += 2;
            }

         ret += (bp - pbase);

#     if defined(DEBUG) && defined(U_STDCPP_ENABLE)
         if (ch == 'O') free(cp);
#     endif

         continue;
         }

case_ustring_v:
      sign = '\0';
      size = (prec < 0 ? (int) u__strlen((const char*)cp, __PRETTY_FUNCTION__) : prec);

      U_INTERNAL_ERROR(size <= (int)(buffer_size - ret), "WE ARE GOING TO OVERFLOW BUFFER at u__vsnprintf() size = %u remaining = %u cp = %.20s buffer_size = %u format = \"%s\"",
                       size, (buffer_size - ret), cp, buffer_size, format_save);

      /* if a width from format is specified, the 0 flag for padding will be ignored... */

      if (width >= 0) flags &= ~ZEROPAD;

      goto next;

case_B: /* extension: print bit conversion of int */
#  ifdef DEBUG
      i  = sizeof(int);
      n  = VA_ARG(int);
      cp = (unsigned char* restrict)&n;

#  if __BYTE_ORDER != __LITTLE_ENDIAN
      cp += sizeof(int);
#  endif

      *bp++ = '<';

      while (true)
         {
#     if __BYTE_ORDER == __LITTLE_ENDIAN
         c = *cp++;
#     else
         c = *--cp;
#     endif

         if (c)
            {
            int j;

#        if __BYTE_ORDER == __LITTLE_ENDIAN
            for (j = 0; j <= 7; ++j)
#        else
            for (j = 7; j >= 0; --j)
#        endif
               {
               *bp++ = '0' + u_test_bit(j,c);
               }
            }
         else
            {
            u_put_unalignedp64(bp, U_MULTICHAR_CONSTANT64('0','0','0','0','0','0','0','0'));

            bp += 8;
            }

         if (--i == 0) break;

         *bp++ = ' ';
         }

      *bp++ = '>';

      ret += sizeof(int) * 8 + sizeof(int) - 1 + 2;
#  endif

      continue;

case_C: /* extension: print formatted char */
      c     = VA_ARG(int);
      pbase = bp;

      *bp++ = '\'';

      if (u__isquote(c) == false) bp += u_sprintc(bp, c);
      else
         {
         if (c == '"') *bp++ = '"';
         else
            {
            *bp++ = '\\';
            *bp++ = '\'';
            }
         }

      *bp++ = '\'';

      ret += (bp - pbase);

      continue;

case_D: /* extension: print date and time in various format */
      if (flags & ALT) t = VA_ARG(time_t); /* flag '#' => var-argument */
      else
         {
         U_gettimeofday; // NB: optimization if it is enough a time resolution of one second...

                         t  = u_now->tv_sec;
         if (width != 8) t += u_now_adjust;
         }

      /**
       *             0  => format: %d/%m/%y
       * with flag  '1' => format:          %T (=> "%H:%M:%S)
       * with flag  '2' => format:          %T (=> "%H:%M:%S) +n days
       * with flag  '3' => format: %d/%m/%Y %T
       * with flag  '4' => format: %d%m%y_%H%M%S_millisec (for file name, backup, etc...)
       * with flag  '5' => format: %a, %d %b %Y %T %Z
       * with flag  '6' => format: %Y/%m/%d
       * with flag  '7' => format: %Y/%m/%d %T
       * with flag  '8' => format: %a, %d %b %Y %T GMT
       * with flag  '9' => format: %d/%m/%y %T
       * with flag '10' => format: %d/%b/%Y:%T %z
       */

      fmtdate =
          (width ==  0 ? "%d/%m/%y"            :
           width <=  2 ? "%T"                  :
           width ==  3 ? "%d/%m/%Y %T"         :
           width ==  4 ? "%d%m%y_%H%M%S"       :
           width ==  5 ? "%a, %d %b %Y %T %Z"  :
           width ==  6 ? "%Y/%m/%d"            :
           width ==  7 ? "%Y/%m/%d %T"         :
           width ==  8 ? "%a, %d %b %Y %T GMT" :
           width ==  9 ? "%d/%m/%y %T"         :
                         "%d/%b/%Y:%T %z");

      len = u_strftime2(bp, 36, fmtdate, t);

      if (width == 2) /* check for days */
         {
         if (flags & ALT &&
             t > U_ONE_DAY_IN_SECOND)
            {
            char tmp[16];
            uint32_t len1;

            (void) sprintf(tmp, " +%ld days", (long)t / U_ONE_DAY_IN_SECOND);

            len1 = u__strlen(tmp, __PRETTY_FUNCTION__);

            u__memcpy(bp+len, tmp, len1, __PRETTY_FUNCTION__);

            len += len1;
            }
         }
      else if (width == 4) /* _millisec */
         {
         char tmp[16];
         uint32_t len1;

#     ifdef ENABLE_THREAD
         if (u_pthread_time) (void) gettimeofday(u_now, 0);
#     endif

         (void) sprintf(tmp, "_%03ld", u_now->tv_usec / 1000L);

         len1 = u__strlen(tmp, __PRETTY_FUNCTION__);

         u__memcpy(bp+len, tmp, len1, __PRETTY_FUNCTION__);

         len += len1;
         }

      bp  += len;
      ret += len;

      continue;

case_H: /* extension: print host name */
      U_INTERNAL_ERROR(u_hostname_len, "HOSTNAME NULL at u__vsnprintf() - CHECK THE PARAMETERS - format = \"%s\"", format_save);

      u__memcpy(bp, u_hostname, u_hostname_len, __PRETTY_FUNCTION__);

      bp  += u_hostname_len;
      ret += u_hostname_len;

      continue;

case_I: /* extension: print off_t */
#  if SIZEOF_OFF_T == 8 && defined(ENABLE_LFS)
      flags |= LLONGINT;
#  endif

      goto case_d;

case_J: /* extension: print U_DATA */

      udata = VA_ARG(struct U_DATA);

      cp   = udata.dptr;
      prec = udata.dsize;

      goto case_ustring_V;

case_L: /* field length modifier */
      flags |= LONGDBL;

      goto rflag;

case_M: /* extension: print memory dump */
#  ifdef DEBUG
      cp = VA_ARG(unsigned char* restrict);
      n  = VA_ARG(int);

      len = u_memory_dump(bp, cp, n);

      bp  += len;
      ret += len;
#  endif

      continue;

case_N: /* extension: print program name */
      u__memcpy(bp, u_progname, u_progname_len, __PRETTY_FUNCTION__);

      bp  += u_progname_len;
      ret += u_progname_len;

      continue;

case_P: /* extension: print process pid */
      u__memcpy(bp, u_pid_str, u_pid_str_len, __PRETTY_FUNCTION__);

      bp  += u_pid_str_len;
      ret += u_pid_str_len;

      continue;

case_Q: /* extension: call exit() or abort() (var-argument is the arg passed to exit) */
      u_flag_exit = VA_ARG(int);

      continue;

case_R: /* extension: print msg - u_getSysError() */
      ccp = VA_ARG(const char* restrict);

      U_INTERNAL_PRINT("ccp = %s", ccp)

      if (ccp)
         {
         len = u__strlen(ccp, __PRETTY_FUNCTION__);

         u__memcpy(bp, ccp, len, __PRETTY_FUNCTION__);

         bp  += len;
         ret += len;
         }

      if ((flags & ALT) == 0)
         {
                           *bp++ = ' ';
         u_put_unalignedp16(bp, U_MULTICHAR_CONSTANT16('-',' '));

         bp  += 2;
         ret += 3;
         }

      if (errno == 0) errno = u_errno;

#  ifdef _MSWINDOWS_
      if (errno < 0)
         {
         errno = - errno;

         ccp = getSysError_w32((uint32_t*)&len);

         u__memcpy(bp, ccp, len, __PRETTY_FUNCTION__);

         bp  += len;
         ret += len;

                           *bp++ = ' ';
         u_put_unalignedp16(bp, U_MULTICHAR_CONSTANT16('-',' '));

         bp  += 2;
         ret += 3;

         MAP_WIN32_ERROR_TO_POSIX
         }
#  endif

      u_getSysError((uint32_t*)&len);

      u__memcpy(bp, u_err_buffer, len, __PRETTY_FUNCTION__);

      bp  += len;
      ret += len;

      continue;

case_T: /* extension: print time_t */
#  if SIZEOF_TIME_T == 8
      flags |= LLONGINT;
#  endif

      goto case_d;

case_U: /* extension: print user name */
      U_INTERNAL_ERROR(u_user_name_len, "USER NAME NULL at u__vsnprintf() - CHECK THE PARAMETERS - format = \"%s\"", format_save);

      u__memcpy(bp, u_user_name, u_user_name_len, __PRETTY_FUNCTION__);

      bp  += u_user_name_len;
      ret += u_user_name_len;

      continue;

case_V: /* extension: print ustring */

      pstr = VA_ARG(struct ustringrep*);

      U_INTERNAL_ASSERT_POINTER(pstr)

      cp   = (unsigned char* restrict) pstr->str;
      prec =                           pstr->_length;

      goto case_ustring_V;

case_W: /* extension: print COLOR (ANSI ESCAPE STR) */
      n = VA_ARG(int);

#  if !defined(_MSWINDOWS_) && !defined(__UNIKERNEL__)
      if (u_is_tty)
         {
         U_INTERNAL_ERROR(n <= BRIGHTWHITE, "INVALID COLOR(%d) at u__vsnprintf() - CHECK THE PARAMETERS - format = \"%s\"", n, format_save);

         len = sizeof(U_RESET_STR) - (n == RESET);

         u__memcpy(bp, tab_color[n], len, __PRETTY_FUNCTION__);

         bp  += len;
         ret += len;
         }
#  endif

      continue;

case_X:
      base     = HEX;
      argument = UARG();

      /* leading 0x/X only if non-zero */

      if ((flags & ALT) &&
          (argument != 0LL))
         {
         flags |= HEXPREFIX;
         }

      /* uint32_t conversions */

nosign: sign = '\0';

      /* ... diouXx conversions ... if a precision is specified, the 0 flag will be ignored */

number: if ((dprec = prec) >= 0) flags &= ~ZEROPAD;

      /* The result of converting a zero value with an explicit precision of zero is no characters */

      cp = buf_number + sizeof(buf_number);

      if ((argument != 0LL) ||
          (prec     != 0))
         {
         /* uint32_t mod is hard, and uint32_t mod by a constant is easier than that by a variable; hence this conditional */

         if (base == OCT)
            {
            do { *--cp = (argument & 7L) + '0'; } while (argument >>= 3L);

            /* handle octal leading 0 */

            if (flags & ALT &&
                *cp != '0')
               {
               *--cp = '0';
               }
            }
         else if (base == HEX)
            {
            const unsigned char* restrict xdigs = (ch == 'X' ? u_hex_upper : u_hex_lower); /* digits for [xX] conversion */

            do { *--cp = xdigs[argument & 15L]; } while (argument /= 16L);
            }
         else
            {
            U_INTERNAL_ASSERT_EQUALS(base, DEC)

            if (LIKELY((flags & THOUSANDS_GROUPED) == 0))
               {
               /**
                * To divide 64-bit numbers and to find remainders on the x86 platform gcc and icc call the libc functions
                * [u]divdi3() and [u]moddi3(), and they call another function in its turn. On FreeBSD it is the qdivrem()
                * function, its source code is about 170 lines of the code. The glibc counterpart is about 150 lines of code.
                *
                * For 32-bit numbers and some divisors gcc and icc use a inlined multiplication and shifts.
                * For example, unsigned "i32 / 10" is compiled to (i32 * 0xCCCCCCCD) >> 35
                */

               if (argument <= UINT_MAX)
                  {
                  uint32_t ui32 = (uint32_t) argument;

                  while (ui32 >= 100U)
                     {
                     cp -= 2;

                     U_NUM2STR16(cp, ui32 % 100U);

                     ui32 /= 100U;
                     }
                     
                  /* Handle last 1-2 digits */

                  if (ui32 < 10U) *--cp = '0' + ui32;
                  else
                     {
                     cp -= 2;

                     U_NUM2STR16(cp, ui32);
                     }
                  }
               else
                  {
                  while (argument >= 100LL)
                     {
                     cp -= 2;

                     U_NUM2STR16(cp, argument % 100LL);

                     argument /= 100LL;
                     }

                  /* Handle last 1-2 digits */

                  if (argument < 10LL) *--cp = '0' + argument;
                  else
                     {
                     cp -= 2;

                     U_NUM2STR16(cp, argument);
                     }
                  }
               }
            else
               {
               n = 1;
   
               while (argument >= 10LL) /* NB: many numbers are 1 digit */
                  {
                  *--cp = (unsigned char)(argument % 10LL) + '0';

                  argument /= 10LL;

                  if ((n++ % 3) == 0) *--cp = ',';
                  }

               *--cp = argument + '0';
               }
            }
         }

      size = (ptrdiff_t)(buf_number + sizeof(buf_number) - cp);

      goto next;

case_Y: /* extension: print u_getSysSignal(signo) */
      u_getSysSignal(VA_ARG(int), (uint32_t*)&len);

      u__memcpy(bp, u_err_buffer, len, __PRETTY_FUNCTION__);

      bp  += len;
      ret += len;

      continue;

case_b: /* extension: print bool */
      n = VA_ARG(int);

      if (n)
         {
         u_put_unalignedp32(bp, U_MULTICHAR_CONSTANT32('t','r','u','e'));

         ret += 4;
         }
      else
         {
                           *bp++ = 'f';
         u_put_unalignedp32(bp, U_MULTICHAR_CONSTANT32('a','l','s','e'));

         ret += 5;
         }

      bp += 4;

      continue;

case_c: /* field conversion specifier */
      *(cp = buf_number) = VA_ARG(int);

      sign = '\0';
      size = 1;

      goto next;

case_d:
      argument = SARG();

      if ((int64_t)argument < 0LL)
         {
         sign     = '-';
         argument = -argument;
         }

      base = DEC;

      goto number;

case_h: /* field length modifier: h hh */
      flags |= SHORTINT;

      goto rflag;

case_j: /* field length modifier - A following integer conversion corresponds to an intmax_t or uintmax_t argument */
/*case_z:  field length modifier - A following integer conversion corresponds to a    size_t or   ssize_t argument */
/*case_t:  field length modifier - A following integer conversion corresponds to a ptrdiff_t              argument */
      goto rflag;

case_l: /* field length modifier: l ll */
      flags |= (flags & LONGINT ? LLONGINT : LONGINT);

      goto rflag;

/* "%n" format specifier, which writes the number of characters written to an address that is passed
 *      as an argument on the stack. It is the format specifier of choice for those performing format string attacks
 *
 * case_n:
 *    if (flags & LLONGINT)
 *       {
 *       long long* p = VA_ARG(long long*);
 *
 *       U_INTERNAL_ERROR(p, "NULL pointer at u__vsnprintf() - CHECK THE PARAMETERS - format = \"%s\"", format_save);
 * 
 *       *p = ret;
 *       }
 *    else if (flags &  LONGINT)
 *       {
 *       long* p = VA_ARG(long*);
 *
 *       U_INTERNAL_ERROR(p, "NULL pointer at u__vsnprintf() - CHECK THE PARAMETERS - format = \"%s\"", format_save);
 *
 *       *p = ret;
 *       }
 *    else if (flags & SHORTINT)
 *       {
 *       short* p = VA_ARG(short*);
 *
 *       U_INTERNAL_ERROR(p, "NULL pointer at u__vsnprintf() - CHECK THE PARAMETERS - format = \"%s\"", format_save);
 *
 *       *p = ret;
 *       }
 *    else
 *       {
 *       int* p = VA_ARG(int*);
 *
 *       U_INTERNAL_ERROR(p, "NULL pointer at u__vsnprintf() - CHECK THE PARAMETERS - format = \"%s\"", format_save);
 *
 *       *p = ret;
 *       }
 * 
 *    continue; // no output
 */

case_o:
      base     = OCT;
      argument = UARG();

      goto nosign;

case_p: /* The argument shall be a pointer to void. The value of the pointer is converted to a sequence of printable characters, in an implementation-defined manner */
#  if defined(HAVE_ARCH64) && defined(U_LINUX)
      argument = (long) VA_ARG(const char* restrict);
#  else
      argument = (long) VA_ARG(const char* restrict) & 0x00000000ffffffffLL;
#  endif

      if (argument == 0)
         {
         *bp++ = '(';

         u_put_unalignedp32(bp, U_MULTICHAR_CONSTANT32('n','i','l',')'));

         bp  += 4;
         ret += 5;

         continue;
         }

      ch     = 'x';
      base   = HEX;
      flags |= HEXPREFIX;

      goto nosign;

case_q: /* field length modifier: quad. This is a synonym for ll */
      flags |= LLONGINT;

      goto rflag;

case_r: /* extension: print u_getExitStatus(exit_value) */
      u_getExitStatus(VA_ARG(int), (uint32_t*)&len);

      u__memcpy(bp, u_err_buffer, len, __PRETTY_FUNCTION__);

      bp  += len;
      ret += len;

      continue;

case_u:
      base     = DEC;
      argument = UARG();

      goto nosign;

case_v: /* extension: print ustring */

      pstr = VA_ARG(struct ustringrep*);

      U_INTERNAL_ASSERT_POINTER(pstr)

      cp   = (unsigned char* restrict) pstr->str;
      prec =                           pstr->_length;

      goto case_ustring_v;

case_w: /* extension: print current working directory */
      U_INTERNAL_ERROR(u_cwd_len, "CURRENT WORKING DIRECTORY NULL at u__vsnprintf() - CHECK THE PARAMETERS - format = \"%s\"", format_save);

      u__memcpy(bp, u_cwd, u_cwd_len, __PRETTY_FUNCTION__);

      bp  += u_cwd_len;
      ret += u_cwd_len;

      continue;

      /**
       * ---------------------------------------------------------------------------------
       * char sign   - sign prefix (' ', '+', '-', or \0)
       * int size    - size of converted field or string
       * int width   - width from format (%8d), or 0
       * int fieldsz - field size expanded by sign, dpad etc
       * int pads    - extra padding size
       * int dpad    - extra 0 padding needed for integers
       * int bpad    - extra blank padding needed
       * int prec    - precision from format (%.3d), or -1
       * int dprec   - a copy of prec if [diouxX], 0 otherwise
       * ---------------------------------------------------------------------------------
       * All reasonable formats wind up here. At this point, `cp' points to
       * a string which (if not flags & LADJUST) should be padded out to
       * 'width' places. If flags & ZEROPAD, it should first be prefixed by any
       * sign or other prefix (%010d = "-000123456"); otherwise, it should be
       * blank padded before the prefix is emitted (%10d = "   -123456"). After
       * any left-hand padding and prefixing, emit zeroes required by a decimal
       * [diouxX] precision, then print the string proper, then emit zeroes
       * required by any leftover floating precision; finally, if LADJUST, pad with blanks
       * ---------------------------------------------------------------------------------
       */
next:
      U_INTERNAL_PRINT("size = %d width = %d prec = %d dprec = %d sign = %c", size, width, prec, dprec, sign)

      dpad = dprec - size; /* compute actual size, so we know how much to pad */

      if (dpad < 0) dpad = 0;

      fieldsz = size + dpad;

      pads = width - fieldsz;

      if (pads < 0) pads = 0;

      U_INTERNAL_PRINT("fieldsz = %d pads = %d dpad = %d", fieldsz, pads, dpad)

      /* adjust ret */

      ret += (width > fieldsz ? width : fieldsz);

      U_INTERNAL_ERROR(ret <= buffer_size,
                       "BUFFER OVERFLOW at u__vsnprintf() ret = %u buffer_size = %u format = \"%s\"", ret, buffer_size, format_save);

      /* right-adjusting blank padding */

      bpad = 0;

      if (pads && (flags & (LADJUST | ZEROPAD)) == 0)
         {
         for (bpad = pads; pads; --pads) *bp++ = ' ';
         }

      /* prefix */

      if (sign != '\0')
         {
         if (bpad) --bp;
         else
            {
            if (pads) --pads;
            else      ++ret;
            }

         *bp++ = sign;
         }

      else if (flags & HEXPREFIX)
         {
         if (bpad) bp -= 2;
         else
            {
            if (pads) pads -= 2;
            else      ret  += 2;
            }

         *bp++ = '0';
         *bp++ = ch;
         }

      /* right-adjusting zero padding */

      if ((flags & (LADJUST | ZEROPAD)) == ZEROPAD)
         {
         for (; pads; --pads) *bp++ = '0';
         }

      /* leading zeroes from decimal precision */

      for (; dpad; --dpad) *bp++ = '0';

      /* the string or number proper */

      for (; size; --size) *bp++ = *cp++;

      /* left-adjusting padding (always blank) */

      if (flags & LADJUST)
         {
         for (; pads; --pads) *bp++ = ' ';
         }
      }

   U_INTERNAL_PRINT("ret = %u buffer_size = %u", ret, buffer_size)

   if (ret < buffer_size) *bp = '\0';

   U_INTERNAL_ERROR(ret <= buffer_size, "BUFFER OVERFLOW at u__vsnprintf() ret = %u buffer_size = %u format = \"%s\"", ret, buffer_size, format_save);

   return ret;
}

uint32_t u__snprintf(char* restrict buffer, uint32_t buffer_size, const char* restrict format, ...)
{
   uint32_t bytes_written;

   va_list argp;
   va_start(argp, format);

   U_INTERNAL_TRACE("u__snprintf(%p,%u,%s)", buffer, buffer_size, format)

   bytes_written = u__vsnprintf(buffer, buffer_size, format, argp);

   va_end(argp);

   return bytes_written;
}

void u__printf(int fd, const char* format, ...)
{
   char buffer[8196];
   uint32_t bytes_written;

   va_list argp;
   va_start(argp, format);

   U_INTERNAL_TRACE("u__printf(%d,%s)", fd, format)

   bytes_written = u__vsnprintf(buffer, sizeof(buffer)-1, format, argp);

   va_end(argp);

   buffer[bytes_written++] = '\n';

#ifdef DEBUG
   if (u_flag_exit < 0) u_printError();
#endif

   (void) write(fd, buffer, bytes_written);

   if (u_flag_exit)
      {
#  ifdef DEBUG
      /* NB: registra l'errore sul file di trace, check stderr per evitare duplicazione messaggio a video */

      if (u_trace_fd > STDERR_FILENO)
         {
         /* check if warning due to syscall */

         if (u_flag_exit != 2 || errno == 0)
            {
            struct iovec iov[1] = { { (caddr_t)buffer, bytes_written } };

            u_trace_writev(iov, 1);
            }
         }
#  endif

      /* check if warning */

      if (u_flag_exit == 2)
         {
         u_flag_exit = 0;

         return;
         }

#  ifdef DEBUG
      if (u_flag_exit < 0)
         {
         if (u_flag_test > 0) /* check if to force continue - test */
            {
            --u_flag_test;

            u_flag_exit = 0;

            return;
            }

         if (u_askForContinue()) return;
         }
#  endif

      u_debug_at_exit();

      exit(u_flag_exit);
      }
}

/* AT EXIT */

vPF u_fns[32];
int u_fns_index;

void u_atexit(vPF function)
{
   int i;

   U_INTERNAL_TRACE("u_atexit(%p)", function)

   U_INTERNAL_ASSERT_POINTER(function)

   for (i = u_fns_index - 1; i >= 0; --i)
      {
      if (u_fns[i] == function) return;
      }

   u_fns[u_fns_index++] = function;
}

void u_unatexit(vPF function)
{
   int i;

   U_INTERNAL_TRACE("u_unatexit(%p)", function)

   U_INTERNAL_ASSERT_POINTER(function)

   for (i = u_fns_index - 1; i >= 0; --i)
      {
      if (u_fns[i] == function)
         {
         u_fns[i] = 0;

         break;
         }
      }
}

void u_exit(void)
{
   int i;

   U_INTERNAL_TRACE("u_exit()")

   U_INTERNAL_PRINT("u_fns_index = %d", u_fns_index)

   for (i = u_fns_index - 1; i >= 0; --i)
      {
      if (u_fns[i])
         {
         U_INTERNAL_PRINT("u_fns[%d] = %p", i, u_fns[i])

         u_fns[i]();
         }
      }
}

/*
#if defined(U_ALL_C) && !defined(DEBUG)
# undef  U_INTERNAL_TRACE
# define U_INTERNAL_TRACE(format,args...)
# undef  U_INTERNAL_PRINT
# define U_INTERNAL_PRINT(format,args...)
#endif
*/
