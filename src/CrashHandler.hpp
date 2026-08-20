#pragma once

// Installs signal handlers that write a crash report (with backtrace) into
// the external data dir so crashes can be diagnosed on-device.
void CrashHandler_Init();
