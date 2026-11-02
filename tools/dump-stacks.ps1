# Copyright (c) 2026 John Pursey
#
# Use of this source code is governed by an MIT-style License that can be found
# in the LICENSE file or at https://opensource.org/licenses/MIT.

# Prints the call stack of each thread in a running program, to see where it is
# stuck when it hangs. Functions and source lines come from the program's PDB.
#
# Usage:
#   tools\dump-stacks.ps1 -ProcessId <id>
#
# For example, for a program the user is running:
#   tools\dump-stacks.ps1 -ProcessId (Get-Process render_check).Id
#
# Each thread is listed with its name, if it has one. A thread running a named
# fiber (such as the fiber job system's) takes the fiber's name. Only the fiber
# running on each thread is shown, not fibers waiting to resume.
#
# This uses Windows' own dbghelp.dll, so it needs no debugger. Each thread is
# suspended briefly while its stack is read.

param(
  [Parameter(Mandatory = $true)]
  [int]$ProcessId
)

$ErrorActionPreference = "Stop"

Add-Type @"
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;

public static class ProgramStackDump {
  [DllImport("kernel32.dll")]
  static extern IntPtr OpenThread(uint access, bool inherit, int tid);
  [DllImport("kernel32.dll")]
  static extern uint SuspendThread(IntPtr thread);
  [DllImport("kernel32.dll")]
  static extern uint ResumeThread(IntPtr thread);
  [DllImport("kernel32.dll")]
  static extern bool GetThreadContext(IntPtr thread, IntPtr context);
  [DllImport("kernel32.dll")]
  static extern bool CloseHandle(IntPtr handle);
  [DllImport("kernel32.dll", CharSet = CharSet.Ansi)]
  static extern IntPtr GetProcAddress(IntPtr module, string name);
  [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
  static extern IntPtr GetModuleHandle(string name);
  [DllImport("kernel32.dll")]
  static extern int GetThreadDescription(IntPtr thread, out IntPtr description);
  [DllImport("kernel32.dll")]
  static extern IntPtr LocalFree(IntPtr memory);

  [DllImport("dbghelp.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  static extern bool SymInitializeW(IntPtr process, string path, bool invade);
  [DllImport("dbghelp.dll")]
  static extern uint SymSetOptions(uint options);
  [DllImport("dbghelp.dll")]
  static extern bool SymCleanup(IntPtr process);
  [DllImport("dbghelp.dll")]
  static extern bool StackWalk64(uint machine, IntPtr process, IntPtr thread,
      IntPtr frame, IntPtr context, IntPtr readMemory, IntPtr functionTable,
      IntPtr moduleBase, IntPtr translateAddress);
  [DllImport("dbghelp.dll")]
  static extern bool SymFromAddrW(IntPtr process, ulong address,
      out ulong displacement, IntPtr symbol);
  [DllImport("dbghelp.dll")]
  static extern bool SymGetLineFromAddrW64(IntPtr process, ulong address,
      out uint displacement, IntPtr line);

  const uint kThreadAllAccess = 0x1FFFFF;
  const uint kMachineAmd64 = 0x8664;
  const int kAddressModeFlat = 3;
  const int kMaxDepth = 64;

  // x64 CONTEXT: its size, and the offsets used. It must be 16-byte aligned.
  const int kContextSize = 1232;
  const int kContextFlags = 0x30;
  const int kContextFull = 0x10000B;  // CONTEXT_AMD64 | CONTROL | INTEGER | FP
  const int kContextRsp = 0x98;
  const int kContextRbp = 0xA0;
  const int kContextRip = 0xF8;

  // STACKFRAME64: each ADDRESS64 is an 8-byte offset, then its mode.
  const int kFrameSize = 512;  // Larger than STACKFRAME64.
  const int kFramePc = 0;
  const int kFrameFrame = 32;
  const int kFrameStack = 48;
  const int kAddressMode = 12;

  // SYMBOL_INFOW, followed by room for its name.
  const int kSymbolSize = 88;
  const int kSymbolMaxNameLength = 80;
  const int kSymbolName = 84;
  const int kMaxNameLength = 1024;

  // IMAGEHLP_LINEW64.
  const int kLineSize = 40;
  const int kLineNumber = 16;
  const int kLineFileName = 24;

  // Zeros for clearing each buffer, as large as the largest (the CONTEXT).
  static readonly byte[] s_zeros = new byte[kContextSize];

  public static string Dump(int pid, string symbolPath) {
    using (Process process = Process.GetProcessById(pid)) {
      IntPtr handle = process.Handle;
      SymSetOptions(0x2 | 0x4 | 0x10);  // UNDNAME | DEFERRED_LOADS | LOAD_LINES
      if (!SymInitializeW(handle, symbolPath, true)) {
        throw new Exception("Could not load symbols (error " +
                            Marshal.GetLastWin32Error() + ").");
      }
      try {
        var text = new StringBuilder();
        foreach (ProcessThread thread in process.Threads) {
          string name;
          List<ulong> stack = WalkStack(handle, thread.Id, out name);
          if (stack == null) {
            continue;
          }
          text.AppendLine("=== Thread " + thread.Id + " " + name);
          if (stack.Count == 0) {
            text.AppendLine("  (could not read the thread's registers)");
          }
          foreach (ulong pc in stack) {
            text.AppendLine("  " + DescribeAddress(handle, pc));
          }
        }
        return text.ToString();
      } finally {
        SymCleanup(handle);
      }
    }
  }

  // Returns the return addresses on a thread's stack, innermost first, or null
  // if the thread has exited. The thread is only suspended while its stack is
  // walked; finding symbols for the addresses can be slow.
  static List<ulong> WalkStack(IntPtr process, int tid, out string name) {
    name = "";
    IntPtr thread = OpenThread(kThreadAllAccess, false, tid);
    if (thread == IntPtr.Zero) {
      return null;
    }
    IntPtr description;
    if (GetThreadDescription(thread, out description) >= 0 &&
        description != IntPtr.Zero) {
      name = Marshal.PtrToStringUni(description);
      LocalFree(description);
    }

    IntPtr dbghelp = GetModuleHandle("dbghelp.dll");
    IntPtr functionTable = GetProcAddress(dbghelp, "SymFunctionTableAccess64");
    IntPtr moduleBase = GetProcAddress(dbghelp, "SymGetModuleBase64");
    IntPtr contextMemory = Marshal.AllocHGlobal(kContextSize + 16);
    IntPtr context = new IntPtr((contextMemory.ToInt64() + 15) & ~15L);
    IntPtr frame = Marshal.AllocHGlobal(kFrameSize);
    var stack = new List<ulong>();
    if (SuspendThread(thread) == uint.MaxValue) {
      CloseHandle(thread);
      Marshal.FreeHGlobal(contextMemory);
      Marshal.FreeHGlobal(frame);
      name += " (could not be suspended)";
      return new List<ulong>();
    }
    try {
      Marshal.Copy(s_zeros, 0, context, kContextSize);
      Marshal.WriteInt32(context, kContextFlags, kContextFull);
      if (!GetThreadContext(thread, context)) {
        return stack;
      }
      Marshal.Copy(s_zeros, 0, frame, kFrameSize);
      SetAddress(frame, kFramePc, Marshal.ReadInt64(context, kContextRip));
      SetAddress(frame, kFrameFrame, Marshal.ReadInt64(context, kContextRbp));
      SetAddress(frame, kFrameStack, Marshal.ReadInt64(context, kContextRsp));
      while (stack.Count < kMaxDepth &&
             StackWalk64(kMachineAmd64, process, thread, frame, context,
                         IntPtr.Zero, functionTable, moduleBase,
                         IntPtr.Zero)) {
        ulong pc = (ulong)Marshal.ReadInt64(frame, kFramePc);
        if (pc == 0) {
          break;
        }
        stack.Add(pc);
      }
      return stack;
    } finally {
      ResumeThread(thread);
      CloseHandle(thread);
      Marshal.FreeHGlobal(contextMemory);
      Marshal.FreeHGlobal(frame);
    }
  }

  // Returns the function and source line for an address, or the address
  // itself if it has no symbol.
  static string DescribeAddress(IntPtr process, ulong pc) {
    string text = "0x" + pc.ToString("x");
    IntPtr symbol = Marshal.AllocHGlobal(kSymbolSize + kMaxNameLength * 2);
    IntPtr line = Marshal.AllocHGlobal(kLineSize);
    try {
      Marshal.Copy(s_zeros, 0, symbol, kSymbolSize);
      Marshal.WriteInt32(symbol, 0, kSymbolSize);
      Marshal.WriteInt32(symbol, kSymbolMaxNameLength, kMaxNameLength);
      ulong symbolDisplacement;
      if (SymFromAddrW(process, pc, out symbolDisplacement, symbol)) {
        text = Marshal.PtrToStringUni(new IntPtr(symbol.ToInt64() + kSymbolName));
      }
      Marshal.Copy(s_zeros, 0, line, kLineSize);
      Marshal.WriteInt32(line, 0, kLineSize);
      uint lineDisplacement;
      if (SymGetLineFromAddrW64(process, pc, out lineDisplacement, line)) {
        string file = Marshal.PtrToStringUni(Marshal.ReadIntPtr(line, kLineFileName));
        text += "  (" + file + ":" + Marshal.ReadInt32(line, kLineNumber) + ")";
      }
    } finally {
      Marshal.FreeHGlobal(symbol);
      Marshal.FreeHGlobal(line);
    }
    return text;
  }

  static void SetAddress(IntPtr frame, int offset, long address) {
    Marshal.WriteInt64(frame, offset, address);
    Marshal.WriteInt32(frame, offset + kAddressMode, kAddressModeFlat);
  }
}
"@

# PDBs are found where each module was built, or beside the program.
$exeDir = Split-Path -Parent (Get-Process -Id $ProcessId).Path
[ProgramStackDump]::Dump($ProcessId, $exeDir)
