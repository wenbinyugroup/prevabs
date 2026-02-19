#include "execu.hpp"
#include "plog.hpp"
#include "PModel.hpp"
#include "utilities.hpp"
#include "globalVariables.hpp"

// #include <gmsh/StringUtils.h>
#include <gmsh_mod/StringUtils.h>
// #include "gmsh/MTriangle.h"

#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <cstdio>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif


// ---------------------------------------------------------------------------
// runCmd: safely spawn an external process without invoking a shell.
//
// On Windows: uses CreateProcess() with an explicitly constructed command
//   line; no shell is involved so metacharacters in paths are safe.
// On POSIX: uses fork()+execvp() with an argument array; the shell is never
//   invoked so there is no injection surface.
//
// Parameters:
//   cmd_name  - executable name or full path (e.g. "vabs", "C:\\tools\\vabs.exe")
//   args      - additional arguments passed to the process (not shell-expanded)
//   pmessage  - logging context
// ---------------------------------------------------------------------------
static void runCmd(
  const std::string &cmd_name,
  const std::vector<std::string> &args
) {
  // Build a display string for logging only.
  std::string display = cmd_name;
  for (const auto &a : args) {
    display += " \"" + a + "\"";
  }
  PLOG(info) << g_msg->message("running: " + display);

#ifdef _WIN32
  // Build the command line string that CreateProcess expects.
  // Each token that may contain spaces is double-quoted.
  // Internal double-quotes in tokens are escaped as \".
  auto quoteArg = [](const std::string &s) -> std::string {
    std::string out = "\"";
    for (char c : s) {
      if (c == '"') { out += "\\\""; }
      else          { out += c; }
    }
    out += "\"";
    return out;
  };

  // Helper: convert a Windows error code to a human-readable string.
  auto winErrorStr = [](DWORD err) -> std::string {
    char buf[512] = {};
    FormatMessageA(
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, err, 0, buf, sizeof(buf), nullptr);
    // Strip trailing newline/whitespace that FormatMessage appends.
    std::string s(buf);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
      s.pop_back();
    return s;
  };

  // Git Bash and MSYS2 update the CRT's environment (what getenv/environ see)
  // but may not sync it to the Windows environment block that CreateProcess
  // and SearchPath use. Reading PATH with _dupenv_s and writing it back with
  // _putenv_s (which calls SetEnvironmentVariable internally) forces that
  // synchronisation so that SearchPath can find executables like VABS that
  // are on the user's PATH but were added through a shell profile.
  {
    char* crt_path = nullptr;
    size_t crt_path_len = 0;
    if (_dupenv_s(&crt_path, &crt_path_len, "PATH") == 0 && crt_path != nullptr) {
      _putenv_s("PATH", crt_path);
      free(crt_path);
    }
  }

  // Resolve the full executable path via SearchPath.
  // Try .exe first; fall back to .bat and .cmd (batch wrappers).
  char exeFullPath[MAX_PATH] = {};
  const char* foundExt = nullptr;
  for (const char* ext : {".exe", ".bat", ".cmd"}) {
    if (SearchPathA(nullptr, cmd_name.c_str(), ext, MAX_PATH, exeFullPath, nullptr) > 0) {
      foundExt = ext;
      break;
    }
  }

  if (!foundExt) {
    DWORD err = GetLastError();
    std::cerr << "ERROR: Cannot find executable '" << cmd_name
              << "' in PATH (tried .exe, .bat, .cmd).\n"
              << "  Make sure it is installed and its directory is on PATH.\n"
              << "  System error " << err << ": " << winErrorStr(err) << "\n"
              << "  Command: " << display << std::endl;
    return;
  }

  // For batch scripts we must invoke through cmd.exe.
  // For .exe files we pass the full path directly as lpApplicationName.
  std::string cmdline;
  const char* lpApplicationName = nullptr;

  bool isBatch = (std::string(foundExt) == ".bat" || std::string(foundExt) == ".cmd");
  if (isBatch) {
    std::string cmdExe = "cmd.exe";
    char comspec[MAX_PATH] = {};
    if (GetEnvironmentVariableA("COMSPEC", comspec, MAX_PATH) > 0) {
      cmdExe = comspec;
    }
    // cmd.exe /c "\"script.bat\" \"arg1\" \"arg2\""
    // The outer quotes wrap the entire command for cmd.exe's /c handling.
    std::string innerCmd = quoteArg(std::string(exeFullPath));
    for (const auto &a : args) {
      innerCmd += " " + quoteArg(a);
    }
    cmdline = cmdExe + " /c \"" + innerCmd + "\"";
    lpApplicationName = nullptr;  // let CreateProcess find cmd.exe
  } else {
    // .exe: use the resolved full path as both lpApplicationName and argv[0].
    cmdline = quoteArg(std::string(exeFullPath));
    for (const auto &a : args) {
      cmdline += " " + quoteArg(a);
    }
    lpApplicationName = exeFullPath;
  }

  // CreateProcess requires a mutable buffer for lpCommandLine.
  std::vector<char> cmdBuf(cmdline.begin(), cmdline.end());
  cmdBuf.push_back('\0');

  STARTUPINFOA si;
  PROCESS_INFORMATION pi;
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  ZeroMemory(&pi, sizeof(pi));

  BOOL ok = CreateProcessA(
    lpApplicationName,  // full path to exe (nullptr for cmd.exe batch case)
    cmdBuf.data(),      // mutable command line
    nullptr,            // process security attributes
    nullptr,            // thread security attributes
    FALSE,              // do not inherit handles
    0,                  // creation flags
    nullptr,            // inherit environment
    nullptr,            // inherit working directory
    &si,
    &pi
  );

  if (!ok) {
    DWORD err = GetLastError();
    std::cerr << "ERROR: Failed to launch '" << std::string(exeFullPath) << "'.\n"
              << "  Command: " << display << "\n"
              << "  System error " << err << ": " << winErrorStr(err) << std::endl;
    return;
  }

  WaitForSingleObject(pi.hProcess, INFINITE);

  DWORD exitCode = 0;
  GetExitCodeProcess(pi.hProcess, &exitCode);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  if (exitCode != 0) {
    std::cerr << "ERROR: Process exited with code " << exitCode
              << ": " << display << std::endl;
  } else {
    PLOG(info) << g_msg->message("command completed successfully.");
  }

#else
  // POSIX: fork + execvp — shell is never involved.
  std::vector<const char *> argv;
  argv.push_back(cmd_name.c_str());
  for (const auto &a : args) {
    argv.push_back(a.c_str());
  }
  argv.push_back(nullptr);

  pid_t pid = fork();
  if (pid < 0) {
    std::cerr << "ERROR: fork() failed for: " << display << std::endl;
    return;
  }

  if (pid == 0) {
    // Child process.
    execvp(cmd_name.c_str(), const_cast<char *const *>(argv.data()));
    // execvp only returns on error.
    std::cerr << "ERROR: execvp() failed for: " << cmd_name << std::endl;
    _exit(127);
  }

  // Parent: wait for child.
  int status = 0;
  waitpid(pid, &status, 0);

  int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  if (exitCode != 0) {
    std::cerr << "ERROR: Process exited with code " << exitCode
              << ": " << display << std::endl;
  } else {
    PLOG(info) << g_msg->message("command completed successfully.");
  }
#endif
}


void runVABS(
  const std::string &cmd_name, const std::vector<std::string> &args,
  Message *pmessage
  ) {
  runCmd(cmd_name, args);
}


void runSC(
  const std::string &cmd_name, const std::vector<std::string> &args,
  Message *pmessage
  ) {
  runCmd(cmd_name, args);
}


void runGmsh(const std::string &fn_geo, const std::string &fn_msh,
             const std::string &fn_opt, Message *pmessage) {
  std::vector<std::string> args;
  if (!fn_geo.empty()) { args.push_back(fn_geo); }
  if (!fn_msh.empty()) { args.push_back(fn_msh); }
  if (!fn_opt.empty()) { args.push_back(fn_opt); }

  runCmd("gmsh", args);
}
