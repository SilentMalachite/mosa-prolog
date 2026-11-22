# Windows だけ。CMake では作れない起こし方で mosa を動かす。標準入力の取っ手が無い（レビュー M-42）、生のコマンドライン
# （レビュー L-38 の 5）、Ctrl+C を受ける自分だけのコンソール（レビュー M-43）。OS に依る C++ は src/platform/ にしか
# 置かないので、Win32 は C# の P/Invoke で呼ぶ。字面は ASCII だけにする。Windows PowerShell 5.1 は BOM の無いファイルを
# ANSI のコードページで読むので、日本語の字は符号位置から作る（化けるのは注釈だけで、動きは変わらない）。
# 使い方: powershell -NoProfile -ExecutionPolicy Bypass -File windows_process_test.ps1 -Case <名前> -Mosa <mosa.exe> -Work <dir>
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

  // milliseconds まで待ち、終わらなければ落とす。"exit N" か "timeout" を返す。
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

  // コンソールを持たせない（DETACHED_PROCESS）。標準出力と標準エラーは log へ。標準入力は withInput なら NUL、
  // そうでなければ取っ手そのものを渡さない（NULL）。GUI から起こしたプログラムと同じ形。
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

  // 自分を 437 の新しいコンソールへ移し、そこで子を起こし、子がコンソールを 65001 にしてから、そのコンソールにだけ
  // Ctrl+C を送る。子の終わり方と、その後のコードページを返す。AllocConsole は自分の標準の取っ手を新しいコンソールへ
  // 替えるので元へ戻し、この台本の文面が ctest に届くようにする。子には新しいコンソールの CONIN$ と CONOUT$ を渡す。
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
    // Ctrl+C を無視するかは子が継ぐので、子を起こす前に通す側にする。
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
    // Ctrl+C はコンソールのすべてのプロセスに届く。自分と子だけのコンソールのときにだけ送る。
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
# $<TARGET_FILE> は / で区切る。コマンドラインの先頭のプログラムの名前は \ にそろえる。
$Mosa = $Mosa -replace '/', '\'
$log = Join-Path $Work "$Case.log"
$quoted = '"' + $Mosa + '"'

switch ($Case) {
  'no_stdin' {
    # M-42。標準入力が無いと _fileno(stdin) は負になる。端末かの確かめがそれを CRT に渡して落ちてはならない。
    $ended = [MosaProcessProbe]::RunDetached("$quoted -g ""write(ok), nl, halt""", $log, $false)
    $text = Read-Log $log
    if ($ended -ne 'exit 0' -or $text -ne "ok`n") { Fail "got [$ended], output [$text]" }
  }
  'argv_quotes' {
    # L-38 の 5。引用符の中の "" は CRT では 1 つの " で、CommandLineToArgvW は引数の数を違えて数える。それでも -- の
    # 後の引数は、ANSI の argv を通らずに UTF-8 のまま argv の旗に届く。
    $word = [string][char]0x65E5 + [char]0x672C
    $tail = [string][char]0x8A9E
    $goal = 'current_prolog_flag(argv, A), write(A), nl, halt'
    $ended = [MosaProcessProbe]::RunDetached("$quoted -g ""$goal"" -- ""$word""""$tail x""", $log, $true)
    $text = Read-Log $log
    $expected = "[$word""$tail x]`n"
    if ($ended -ne 'exit 0' -or $text -ne $expected) { Fail "got [$ended], output [$text], expected [$expected]" }
  }
  'ctrl_c' {
    # M-43。Ctrl+C は atexit を通らずに mosa を終える。それでもコンソールのコードページは元に戻る。
    $ended = [MosaProcessProbe]::CtrlC("$quoted -g ""repeat, fail""")
    if (-not $ended.EndsWith('code pages 437 437')) { Fail "got [$ended]" }
  }
  default { Fail "unknown case" }
}
Write-Output "windows_process_test ${Case}: ok"
exit 0
