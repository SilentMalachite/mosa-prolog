# Windows only. Starts mosa the way CMake cannot: with no standard input (review M-42), with a raw command line
# (review L-38 point 5) and in a console of its own that gets Ctrl+C (review M-43). Win32 calls go through C# P/Invoke,
# because OS-dependent C++ is kept to src/platform/.
# This file is ASCII only, comments included, as an exception to the Japanese-comment rule (open-items): Windows
# PowerShell 5.1 reads a file without a BOM in the ANSI code page, and under 932 a UTF-8 byte at the end of a comment
# can be read as a lead byte that takes the newline, so the next line of code would join the comment. The Japanese
# text the tests need is built from code points.
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File windows_process_test.ps1 -Case <name> -Mosa <mosa.exe> -Work <dir>
param(
  [Parameter(Mandatory = $true)][string]$Case,
  [Parameter(Mandatory = $true)][string]$Mosa,
  [Parameter(Mandatory = $true)][string]$Work
)
$ErrorActionPreference = 'Stop'

Add-Type -Language CSharp -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

public static class MosaProcessProbe {
  [StructLayout(LayoutKind.Sequential)]
  public struct StartupInfo {
    public int cb; public IntPtr lpReserved; public IntPtr lpDesktop; public IntPtr lpTitle;
    public int dwX; public int dwY; public int dwXSize; public int dwYSize;
    public int dwXCountChars; public int dwYCountChars; public int dwFillAttribute; public int dwFlags;
    public short wShowWindow; public short cbReserved2; public IntPtr lpReserved2;
    public IntPtr hStdInput; public IntPtr hStdOutput; public IntPtr hStdError;
  }
  [StructLayout(LayoutKind.Sequential)]
  public struct ProcessInformation { public IntPtr hProcess; public IntPtr hThread; public int dwProcessId; public int dwThreadId; }
  [StructLayout(LayoutKind.Sequential)]
  public struct SecurityAttributes { public int nLength; public IntPtr lpSecurityDescriptor; public int bInheritHandle; }

  [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
  static extern bool CreateProcessW(string application, StringBuilder commandLine, IntPtr processAttributes,
    IntPtr threadAttributes, bool inheritHandles, uint creationFlags, IntPtr environment, string currentDirectory,
    ref StartupInfo startupInfo, out ProcessInformation processInformation);
  [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
  static extern IntPtr CreateFileW(string name, uint access, uint share, ref SecurityAttributes attributes,
    uint disposition, uint flags, IntPtr template);
  [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);
  [DllImport("kernel32.dll")] static extern bool GetExitCodeProcess(IntPtr process, out uint code);
  [DllImport("kernel32.dll")] static extern bool TerminateProcess(IntPtr process, uint code);
  [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
  [DllImport("kernel32.dll")] static extern bool FreeConsole();
  [DllImport("kernel32.dll", SetLastError = true)] static extern bool AllocConsole();
  [DllImport("kernel32.dll")] static extern uint GetConsoleCP();
  [DllImport("kernel32.dll")] static extern uint GetConsoleOutputCP();
  [DllImport("kernel32.dll")] static extern bool SetConsoleCP(uint page);
  [DllImport("kernel32.dll")] static extern bool SetConsoleOutputCP(uint page);
  [DllImport("kernel32.dll")] static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
  [DllImport("kernel32.dll")] static extern bool GenerateConsoleCtrlEvent(uint ctrlEvent, uint processGroup);
  [DllImport("kernel32.dll")] static extern uint GetConsoleProcessList(uint[] list, uint count);
  [DllImport("kernel32.dll")] static extern IntPtr GetStdHandle(int which);
  [DllImport("kernel32.dll")] static extern bool SetStdHandle(int which, IntPtr handle);

  const uint GenericRead = 0x80000000, GenericWrite = 0x40000000;
  const uint ShareRead = 1, ShareWrite = 2, CreateAlways = 2, OpenExisting = 3, NormalFile = 0x80;
  const int UseStdHandles = 0x100;
  const uint DetachedProcess = 0x8, CtrlCEvent = 0;
  const int StdInput = -10, StdOutput = -11, StdError = -12;
  static readonly IntPtr Invalid = new IntPtr(-1);

  // Waits up to milliseconds, then kills the child. Returns "exit N" or "timeout".
  static string Finish(ProcessInformation process, uint milliseconds) {
    string result;
    if (WaitForSingleObject(process.hProcess, milliseconds) != 0) {
      TerminateProcess(process.hProcess, 1);
      WaitForSingleObject(process.hProcess, 10000);
      result = "timeout";
    } else {
      uint code;
      GetExitCodeProcess(process.hProcess, out code);
      result = "exit " + code.ToString();
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return result;
  }

  static IntPtr Inheritable(string path, uint access, uint disposition) {
    SecurityAttributes attributes = new SecurityAttributes();
    attributes.nLength = Marshal.SizeOf(typeof(SecurityAttributes));
    attributes.bInheritHandle = 1;
    return CreateFileW(path, access, ShareRead | ShareWrite, ref attributes, disposition, NormalFile, IntPtr.Zero);
  }

  // No console (DETACHED_PROCESS). Standard output and error go to log. Standard input is NUL when withInput,
  // and no handle at all (NULL) otherwise, as for a program started from a GUI.
  public static string RunDetached(string commandLine, string log, bool withInput) {
    IntPtr output = Inheritable(log, GenericWrite, CreateAlways);
    if (output == Invalid) return "cannot open the log: " + Marshal.GetLastWin32Error().ToString();
    IntPtr input = IntPtr.Zero;
    if (withInput) {
      input = Inheritable("NUL", GenericRead, OpenExisting);
      if (input == Invalid) { CloseHandle(output); return "cannot open NUL"; }
    }
    StartupInfo startup = new StartupInfo();
    startup.cb = Marshal.SizeOf(typeof(StartupInfo));
    startup.dwFlags = UseStdHandles;
    startup.hStdInput = input;
    startup.hStdOutput = output;
    startup.hStdError = output;
    ProcessInformation process;
    bool started = CreateProcessW(null, new StringBuilder(commandLine), IntPtr.Zero, IntPtr.Zero, true,
                                  DetachedProcess, IntPtr.Zero, null, ref startup, out process);
    int error = Marshal.GetLastWin32Error();
    CloseHandle(output);
    if (input != IntPtr.Zero) CloseHandle(input);
    if (!started) return "cannot start: " + error.ToString();
    return Finish(process, 120000);
  }

  // Moves this process to a new console at code page 437, starts the child in it, waits until the child has set the
  // console to 65001, and sends Ctrl+C to that console only. Returns how the child ended and the code pages after it.
  // AllocConsole points this process's standard handles at the new console; they are put back so that what this script
  // writes still reaches ctest. The child gets the new console's CONIN$ and CONOUT$.
  public static string CtrlC(string commandLine) {
    IntPtr savedInput = GetStdHandle(StdInput), savedOutput = GetStdHandle(StdOutput), savedError = GetStdHandle(StdError);
    FreeConsole();
    bool allocated = AllocConsole();
    int allocError = Marshal.GetLastWin32Error();
    SetStdHandle(StdInput, savedInput);
    SetStdHandle(StdOutput, savedOutput);
    SetStdHandle(StdError, savedError);
    if (!allocated) return "AllocConsole failed: " + allocError.ToString();
    if (!SetConsoleCP(437) || !SetConsoleOutputCP(437)) return "cannot set the code page 437";
    IntPtr consoleInput = Inheritable("CONIN$", GenericRead | GenericWrite, OpenExisting);
    IntPtr consoleOutput = Inheritable("CONOUT$", GenericRead | GenericWrite, OpenExisting);
    if (consoleInput == Invalid || consoleOutput == Invalid) return "cannot open CONIN$ or CONOUT$";
    // The child inherits whether Ctrl+C is ignored, so let it through before the child starts.
    SetConsoleCtrlHandler(IntPtr.Zero, false);
    StartupInfo startup = new StartupInfo();
    startup.cb = Marshal.SizeOf(typeof(StartupInfo));
    startup.dwFlags = UseStdHandles;
    startup.hStdInput = consoleInput;
    startup.hStdOutput = consoleOutput;
    startup.hStdError = consoleOutput;
    ProcessInformation process;
    bool started = CreateProcessW(null, new StringBuilder(commandLine), IntPtr.Zero, IntPtr.Zero, true, 0, IntPtr.Zero,
                                  null, ref startup, out process);
    int startError = Marshal.GetLastWin32Error();
    CloseHandle(consoleInput);
    CloseHandle(consoleOutput);
    if (!started) return "cannot start: " + startError.ToString();
    for (int tries = 0; tries < 600 && GetConsoleOutputCP() != 65001; ++tries) Thread.Sleep(100);
    if (GetConsoleOutputCP() != 65001) return "mosa did not set 65001 (" + Finish(process, 0) + ")";
    // Ctrl+C reaches every process on the console. Send it only if the console holds just this process and the child.
    uint[] attached = new uint[8];
    uint count = GetConsoleProcessList(attached, (uint)attached.Length);
    if (count != 2) return "the console has " + count.ToString() + " processes (" + Finish(process, 0) + ")";
    SetConsoleCtrlHandler(IntPtr.Zero, true);
    if (!GenerateConsoleCtrlEvent(CtrlCEvent, 0)) return "GenerateConsoleCtrlEvent failed (" + Finish(process, 0) + ")";
    string ended = Finish(process, 60000);
    return ended + ", code pages " + GetConsoleCP().ToString() + " " + GetConsoleOutputCP().ToString();
  }
}
'@

function Fail([string]$message) {
  [Console]::Error.WriteLine("windows_process_test ${Case}: $message")
  exit 1
}

function Read-Log([string]$path) {
  if (-not (Test-Path -LiteralPath $path)) { return '' }
  return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

New-Item -ItemType Directory -Force -Path $Work | Out-Null
# $<TARGET_FILE> uses /. The program name at the head of the command line gets \.
$Mosa = $Mosa -replace '/', '\'
$log = Join-Path $Work "$Case.log"
$quoted = '"' + $Mosa + '"'

switch ($Case) {
  'no_stdin' {
    # M-42: with no standard input, _fileno(stdin) is negative. The terminal check must not hand it to the CRT.
    $ended = [MosaProcessProbe]::RunDetached("$quoted -g ""write(ok), nl, halt""", $log, $false)
    $text = Read-Log $log
    if ($ended -ne 'exit 0' -or $text -ne "ok`n") { Fail "got [$ended], output [$text]" }
  }
  'argv_quotes' {
    # L-38 point 5: "" inside quotes is one quote for the CRT, and CommandLineToArgvW counts the arguments
    # differently. The argument after -- must still reach the argv flag as UTF-8, not through the ANSI argv.
    $word = [string][char]0x65E5 + [char]0x672C
    $tail = [string][char]0x8A9E
    $goal = 'current_prolog_flag(argv, A), write(A), nl, halt'
    $ended = [MosaProcessProbe]::RunDetached("$quoted -g ""$goal"" -- ""$word""""$tail x""", $log, $true)
    $text = Read-Log $log
    $expected = "[$word""$tail x]`n"
    if ($ended -ne 'exit 0' -or $text -ne $expected) { Fail "got [$ended], output [$text], expected [$expected]" }
  }
  'ctrl_c' {
    # M-43: Ctrl+C ends mosa without its atexit functions. The console code pages must still be put back.
    $ended = [MosaProcessProbe]::CtrlC("$quoted -g ""repeat, fail""")
    if (-not $ended.EndsWith('code pages 437 437')) { Fail "got [$ended]" }
  }
  default { Fail "unknown case" }
}
Write-Output "windows_process_test ${Case}: ok"
exit 0
