#include "execu_utils.hpp"

#ifdef _WIN32
WaitResult waitWithTimeout(HANDLE hProcess, DWORD timeout_ms) {
  DWORD result = WaitForSingleObject(hProcess, timeout_ms);
  return (result == WAIT_TIMEOUT) ? WaitResult::TimedOut : WaitResult::Completed;
}
#endif
