include(CheckIncludeFile)

check_include_file("arpa/inet.h"      HAVE_ARPA_INET_H)
check_include_file("dirent.h"         HAVE_DIRENT_H)
check_include_file("execinfo.h"       HAVE_EXECINFO_H)
check_include_file("fcntl.h"          HAVE_FCNTL_H)
check_include_file("inttypes.h"       HAVE_INTTYPES_H)
check_include_file("limits.h"         HAVE_LIMITS_H)
check_include_file("linux/ppdev.h"    HAVE_LINUX_PPDEV_H)
check_include_file("memory.h"         HAVE_MEMORY_H)
check_include_file("netdb.h"          HAVE_NETDB_H)
check_include_file("netinet/in.h"     HAVE_NETINET_IN_H)
check_include_file("regex.h"          HAVE_REGEX_H)
check_include_file("stdbool.h"        HAVE_STDBOOL_H)
check_include_file("stdint.h"         HAVE_STDINT_H)
check_include_file("stdio.h"          HAVE_STDIO_H)
check_include_file("stdlib.h"         HAVE_STDLIB_H)
check_include_file("string.h"         HAVE_STRING_H)
check_include_file("strings.h"        HAVE_STRINGS_H)
check_include_file("sys/ioctl.h"      HAVE_SYS_IOCTL_H)
check_include_file("sys/param.h"      HAVE_SYS_PARAM_H)
check_include_file("sys/select.h"     HAVE_SYS_SELECT_H)
check_include_file("sys/socket.h"     HAVE_SYS_SOCKET_H)
check_include_file("sys/stat.h"       HAVE_SYS_STAT_H)
check_include_file("sys/time.h"       HAVE_SYS_TIME_H)
check_include_file("sys/types.h"      HAVE_SYS_TYPES_H)
check_include_file("sys/utsname.h"    HAVE_SYS_UTSNAME_H)
check_include_file("termios.h"        HAVE_TERMIOS_H)
check_include_file("unistd.h"         HAVE_UNISTD_H)
check_include_file("values.h"         HAVE_VALUES_H)
check_include_file("wchar.h"          HAVE_WCHAR_H)

include(CheckFunctionExists)

check_function_exists(clock_gettime   HAVE_CLOCK_GETTIME)
check_function_exists(dcgettext       HAVE_DCGETTEXT)
check_function_exists(dlopen          HAVE_DLOPEN)
check_function_exists(getaddrinfo     HAVE_GETADDRINFO)
check_function_exists(gethostbyname   HAVE_GETHOSTBYNAME)
check_function_exists(gettext         HAVE_GETTEXT)
check_function_exists(gmtime_r        HAVE_GMTIME_R)
check_function_exists(hstrerror       HAVE_HSTRERROR)
check_function_exists(localtime_r     HAVE_LOCALTIME_R)
check_function_exists(memmove         HAVE_MEMMOVE)
check_function_exists(memset          HAVE_MEMSET)
check_function_exists(mkdir           HAVE_MKDIR)
check_function_exists(select          HAVE_SELECT)
check_function_exists(sem_timedwait   HAVE_SEM_TIMEDWAIT)
check_function_exists(sem_unlink      HAVE_SEM_UNLINK)
check_function_exists(setenv          HAVE_SETENV)
check_function_exists(snprintf        HAVE_SNPRINTF)
check_function_exists(socket          HAVE_SOCKET)
check_function_exists(socketpair      HAVE_SOCKETPAIR)
check_function_exists(strcasecmp      HAVE_STRCASECMP)
check_function_exists(strcasestr      HAVE_STRCASESTR)
check_function_exists(strchr          HAVE_STRCHR)
check_function_exists(strdup          HAVE_STRDUP)
check_function_exists(strerror        HAVE_STRERROR)
check_function_exists(strftime        HAVE_STRFTIME)
check_function_exists(strlcpy         HAVE_STRLCPY)
check_function_exists(strncasecmp     HAVE_STRNCASECMP)
check_function_exists(strncmp         HAVE_STRNCMP)
check_function_exists(strncpy         HAVE_STRNCPY)
check_function_exists(strnlen         HAVE_STRNLEN)
check_function_exists(strrchr         HAVE_STRRCHR)
check_function_exists(strstr          HAVE_STRSTR)
check_function_exists(strtol          HAVE_STRTOL)
check_function_exists(uname           HAVE_UNAME)
check_function_exists(unsetenv        HAVE_UNSETENV)
check_function_exists(vsnprintf       HAVE_VSNPRINTF)

include(CheckSymbolExists)

check_symbol_exists(
    O_CLOEXEC
    "fcntl.h"
    HAVE_O_CLOEXEC
)

include(CheckCSourceCompiles)

check_c_source_compiles(
    "
    int main(void)
    {
        _Bool b = 1;
        return b ? 0 : 1;
    }
    "
    HAVE__BOOL
)

include(CheckCXXSourceCompiles)

check_cxx_source_compiles(
    "
    #include <functional>
    #include <string>

    int main()
    {
        std::hash<std::string> h;
        return h(\"fldigi\") == 0;
    }
    "
    HAVE_STD_HASH
)
