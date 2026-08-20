#include "CrashHandler.hpp"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <execinfo.h>

#include "FileSystem.hpp"

static void WriteCrash(int sig, const char *name)
{
    FILE *f = fopen(FileSystem::GetBasePath("crash_report.txt").c_str(), "a");
    if (f)
    {
        fprintf(f, "===== CRASH: %s (sig %d) =====\n", name, sig);
        fflush(f);
    }

    void *bt[64];
    int n = backtrace(bt, 64);
    char **symbols = backtrace_symbols(bt, n);
    if (f)
    {
        for (int i = 0; i < n; i++)
        {
            fprintf(f, "  #%02d %s\n", i, symbols ? symbols[i] : "?");
        }
        fclose(f);
    }
    if (symbols)
    {
        free(symbols);
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
