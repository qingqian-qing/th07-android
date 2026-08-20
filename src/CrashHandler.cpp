#include "CrashHandler.hpp"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "FileSystem.hpp"

static void WriteCrash(int sig, const char *name)
{
    FILE *f = fopen(FileSystem::GetBasePath("crash_report.txt").c_str(), "a");
    if (f)
    {
        fprintf(f, "===== CRASH: %s (sig %d) =====\n", name, sig);
        fprintf(f, "  #00 %p\n", __builtin_return_address(0));
        for (int i = 1; i < 20; i++)
        {
            void *ra = __builtin_return_address(i);
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
