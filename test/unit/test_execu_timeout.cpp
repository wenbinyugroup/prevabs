// test_execu_timeout.cpp
// Unit tests for the waitWithTimeout helper (Windows only).
//
// Uses Windows event objects (not real processes) so the tests are fast and
// do not depend on any external executables.

#include "catch_amalgamated.hpp"

#ifdef _WIN32
#include "execu_utils.hpp"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Create a manual-reset event and immediately set it (signaled).
static HANDLE makeSignaledEvent() {
  HANDLE h = CreateEvent(nullptr, TRUE, TRUE, nullptr);
  REQUIRE(h != nullptr);
  return h;
}

// Create a manual-reset event that is NOT signaled.
static HANDLE makeUnsignaledEvent() {
  HANDLE h = CreateEvent(nullptr, TRUE, FALSE, nullptr);
  REQUIRE(h != nullptr);
  return h;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("waitWithTimeout: already-signaled handle returns Completed", "[execu][error]") {
  HANDLE evt = makeSignaledEvent();
  WaitResult r = waitWithTimeout(evt, 1000);
  CloseHandle(evt);
  CHECK(r == WaitResult::Completed);
}

TEST_CASE("waitWithTimeout: unsignaled handle with short timeout returns TimedOut", "[execu][error]") {
  HANDLE evt = makeUnsignaledEvent();
  // 10 ms — the event will never be signaled.
  WaitResult r = waitWithTimeout(evt, 10);
  CloseHandle(evt);
  CHECK(r == WaitResult::TimedOut);
}

TEST_CASE("waitWithTimeout: INFINITE on signaled handle returns Completed", "[execu][error]") {
  HANDLE evt = makeSignaledEvent();
  WaitResult r = waitWithTimeout(evt, INFINITE);
  CloseHandle(evt);
  CHECK(r == WaitResult::Completed);
}

#endif // _WIN32
