#pragma once

#ifdef _WIN32
#include <windows.h>

// Result of a timed wait on a process handle.
enum class WaitResult { Completed, TimedOut };

// Wait for hProcess up to timeout_ms milliseconds.
// Pass INFINITE to wait without a limit (same as before).
// The caller is responsible for TerminateProcess on TimedOut.
WaitResult waitWithTimeout(HANDLE hProcess, DWORD timeout_ms);
#endif
