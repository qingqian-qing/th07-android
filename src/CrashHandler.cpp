#include "CrashHandler.hpp"

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "FileSystem.hpp"

// Collect return addresses at fixed depths (__builtin_return_address needs a
// constant argument on clang).
static const void *GetRA(int depth)
{
    switch (depth)
    {
    case 0:  return __builtin_return_address(0);
    case 1:  return __builtin_return_address(1);
    case 2:  return __builtin_return_address(2);
    case 3:  return __builtin_return_address(3);
    case 4:  return __builtin_return_address(4);
    case 5:  return __builtin_return_address(5);
    case 6:  return __builtin_return_address(6);
    case 7:  return __builtin_return_address(7);
    case 8:  return __builtin_return_address(8);
    case 9:  return __builtin_return_address(9);
    case 10: return __builtin_return_address(10);
    case 11: return __builtin_return_address(11);
    case 12: return __builtin_return_address(12);
    case 13: return __builtin_return_address(13);
    case 14: return __builtin_return_address(14);
    case 15: return __builtin_return_address(15);
    default: return nullptr;
    }
}

static void WriteCrash(int sig, const char *name)
{
    FILE *f = fopen(FileSystem::GetBasePath("crash_report.txt").c_str(), "a");
    if (f)
    {
        fprintf(f, "===== CRASH: %s (sig %d) =====\n", name, sig);
        for (int i = 0; i < 16; i++)
        {
            const void *ra = GetRA(i);
            if (!ra)
                break;
            fprintf(f, "  #%02d %p\n", i, ra);
        }
        fclose(f);
    }
    _exit(1);
}

static void HandlerSIGSEGV(int) { WriteCrash(SIGSEGV, "SIGSEGV"); }
static void HandlerSIGABRT(int) { WriteCrash(SIGABRT, "SIGABRT"); }
static void HandlerSIGBUS(int)  { WriteCrash(SIGBUS,  "SIGBUS");  }
static void HandlerSIGFPE(int)  { WriteCrash(SIGFPE,  "SIGFPE");  }

void CrashHandler_Init()
{
    signal(SIGSEGV, HandlerSIGSEGV);
    signal(SIGABRT, HandlerSIGABRT);
    signal(SIGBUS, HandlerSIGBUS);
    signal(SIGFPE, HandlerSIGFPE);
}
