#include "CrashHandler.hpp"

#include <signal.h>
#include <ucontext.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>

#include "FileSystem.hpp"

#if defined(__aarch64__)
#define UC_PC(uc) ((uintptr_t)(uc)->uc_mcontext.pc)
#define UC_FP(uc) ((uintptr_t)(uc)->uc_mcontext.regs[29])
#define UC_LR(uc) ((uintptr_t)(uc)->uc_mcontext.regs[30])
#elif defined(__arm__)
#define UC_PC(uc) ((uintptr_t)(uc)->uc_mcontext.arm_pc)
#define UC_FP(uc) ((uintptr_t)(uc)->uc_mcontext.arm_fp)
#define UC_LR(uc) ((uintptr_t)(uc)->uc_mcontext.arm_lr)
#else
#define UC_PC(uc) ((uintptr_t)0)
#define UC_FP(uc) ((uintptr_t)0)
#define UC_LR(uc) ((uintptr_t)0)
#endif

static void Append(int fd, const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0)
    {
        write(fd, buf, (size_t)(n < (int)sizeof(buf) ? n : sizeof(buf) - 1));
    }
}

// Read memory safely via /proc/self/mem (never faults inside the handler).
static int SafeRead(uintptr_t addr, void *out, size_t sz)
{
    if (addr < 0x10000 || addr > 0x7fffffffffffULL - sz)
    {
        return -1;
    }
    int mfd = open("/proc/self/mem", O_RDONLY);
    if (mfd < 0)
    {
        return -1;
    }
    ssize_t r = pread(mfd, out, sz, (off_t)addr);
    close(mfd);
    return r == (ssize_t)sz ? 0 : -1;
}

static void DumpMaps(int fd)
{
    int mfd = open("/proc/self/maps", O_RDONLY);
    if (mfd < 0)
    {
        return;
    }
    Append(fd, "----- /proc/self/maps -----\n");
    char buf[4096];
    ssize_t n;
    while ((n = read(mfd, buf, sizeof(buf))) > 0)
    {
        write(fd, buf, (size_t)n);
    }
    close(mfd);
}

static void WriteCrash(int sig, siginfo_t *si, void *uctx)
{
    int fd = open(FileSystem::GetBasePath("crash_report.txt").c_str(),
                  O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0)
    {
        _exit(1);
    }

    Append(fd, "\n===== CRASH: sig %d =====\n", sig);
    if (si)
    {
        Append(fd, "si_code=%d si_addr=%p\n", si->si_code, si->si_addr);
    }

    ucontext_t *uc = (ucontext_t *)uctx;
    uintptr_t pc = UC_PC(uc);
    uintptr_t fp = UC_FP(uc);
    uintptr_t lr = UC_LR(uc);
    Append(fd, "PC=%p FP=%p LR=%p\n", (void *)pc, (void *)fp, (void *)lr);

    Append(fd, "----- backtrace (FP/LR chain) -----\n");
    Append(fd, "#00 %p  <- crash PC\n", (void *)pc);
    Append(fd, "#01 %p  <- LR\n", (void *)lr);
    uintptr_t cur = fp;
    int depth = 2;
    for (; depth < 32; depth++)
    {
        uintptr_t nextFp, nextLr;
        if (SafeRead(cur, &nextFp, sizeof(nextFp)) != 0 ||
            SafeRead(cur + 8, &nextLr, sizeof(nextLr)) != 0)
        {
            break;
        }
        if (nextLr == 0 || nextFp == 0 || nextFp <= cur)
        {
            break;
        }
        Append(fd, "#%02d %p\n", depth, (void *)nextLr);
        cur = nextFp;
    }
    if (depth == 32)
    {
        Append(fd, "(backtrace truncated at 32 frames)\n");
    }

    DumpMaps(fd);
    close(fd);
    _exit(1);
}

static void Handler(int sig, siginfo_t *si, void *uctx)
{
    WriteCrash(sig, si, uctx);
}

void CrashHandler_Init()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = Handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
}
