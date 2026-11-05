# M01 Process Scheduler and CLI

CSOPESY MCO1, Group 8. A console emulator with FCFS and Round Robin scheduling, process screens, randomized process instructions, and CPU utilization reports.

Developers: Fred Abenojar, Mary Faye Caya, Deo Zamir Diamante, and Gerylyn Guiller.

The application entry file is `main.cpp`. The shared Visual Studio solution is `M01-ProcessSchedulerCLI.slnx`; it references `M01-ProcessSchedulerCLI.vcxproj` using a relative path.

## Build and run

Use Windows PowerShell and one C++20 compiler:

- MinGW-w64: `g++.exe` must be on PATH.
- Visual Studio with Desktop development with C++: open **Developer PowerShell for Visual Studio** so `cl.exe` is available.

Open PowerShell in the repository folder and run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\run.ps1
```

The script rebuilds the source and launches `build\ProcessScheduler.exe` only if compilation succeeds. It runs from the project folder so `config.txt` is found. The execution-policy option applies only to this PowerShell process.

Useful options:

```powershell
# Compile without launching.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\run.ps1 -BuildOnly

# Build and run the regression checks.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\run.ps1 -Test

# Select Visual Studio's compiler in Developer PowerShell.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\run.ps1 -Compiler msvc
```

The default compiler choice is g++ when available, otherwise cl. Both compiler paths use C++20. Close a running emulator before rebuilding it. Build errors stop the script; do not open an older executable as a workaround.

For the Visual Studio interface, open the `.slnx` solution (or the `.vcxproj` directly), choose Debug/x64, rebuild, and run without debugging. The shared project sets its working directory to `$(ProjectDir)` so `config.txt` is found. An installed toolset compatible with the project's v145 setting is needed; the script can also use MinGW-w64 independently of the project file.

## Team workflow

After pulling changes, run `run.ps1` to rebuild on your own computer. Git transfers source code; it does not compile it. Opening an old `run.exe` or another copied executable can show old behavior even when your source is current.

Commit the source/header files, Visual Studio project and filters, `config.txt`, `.gitignore`, `run.ps1`, the READMEs, and `tests/`. Executables, `build/`, `x64/`, `.vs/`, per-computer `.vcxproj.user` settings, and `csopesy-log.txt` are generated/local files and are not tracked. The cleanup keeps this computer's local copies. The shared project uses its own folder for configuration, with no developer's absolute path in its run settings.

After pulling the cleanup, teammates must rebuild rather than open an old executable. If Git reports local changes in generated files while pulling, keep any needed local copies before resolving those changes; do not discard source edits.

## Commands

Before initialization, only `initialize` and `exit` are recognized. `initialize` reads all seven settings from `config.txt`; failed validation leaves the system uninitialized.

| Main menu command | Behavior |
| --- | --- |
| `initialize` | Load configuration and start the background scheduler. |
| `screen -s <name>` | Create a uniquely named process and open its screen. |
| `screen -r <name>` | Reopen an unfinished process. Finished/missing names report not found. |
| `screen -ls` | Show current core utilization and all active/finished processes. |
| `scheduler-start` | Generate one process every configured number of CPU ticks. |
| `scheduler-stop` | Stop generation; existing processes keep executing. |
| `report-util` | Display a report and save that exact snapshot to `csopesy-log.txt`. |
| `help` | Show commands. |
| `exit` | Stop the scheduler and close the emulator. |

Inside a process screen, `process-smi` displays updated information and stored PRINT logs; `exit` returns to the main menu. Entering a screen clears the console and immediately shows its details. Returning to the main menu clears the process screen and redraws the complete developer header. The startup display is compact; use `help` after initialization for the command list. Windows Terminal uses virtual-terminal clearing, with a visible-window fallback for older consoles. A finished process can still be inspected while its screen remains open, but cannot be reopened after leaving it.

A quick check: run `initialize`, then `screen -s demo`. Wait a few seconds and run `process-smi`. Use `exit` to return, then `screen -ls` and `report-util`. To check generation, run `scheduler-start`, wait, then `scheduler-stop` and `screen -ls`.

## Configuration

The checked-in `config.txt` uses short demo workloads: four cores, RR with a five-tick quantum, a process every ten ticks, 20-40 instructions per process, and no execution delay. These values are for demonstrations; replace them with the assessment's supplied settings when needed.

Each nonblank line contains one setting and one value. Quotes around `fcfs`/`rr` are optional. Unknown, repeated, missing, negative, malformed, and out-of-range values are rejected with an explanation.

| Setting | Allowed values |
| --- | --- |
| `num-cpu` | 1 through 128 |
| `scheduler` | `fcfs` or `rr` |
| `quantum-cycles` | 1 through 4,294,967,296; used by RR |
| `batch-process-freq` | 1 through 4,294,967,296 CPU ticks |
| `min-ins`, `max-ins` | 1 through 4,294,967,296; minimum must not exceed maximum |
| `delays-per-exec` | 0 through 4,294,967,296 CPU ticks |

Configuration changes take effect when a new emulator session runs `initialize`.

## Scheduling and instructions

One background worker advances a shared CPU tick every 100 milliseconds and advances each simulated core during that tick. FCFS keeps a process until it finishes or sleeps. RR gives it at most `quantum-cycles` ticks per turn. Busy delay ticks consume the current turn; SLEEP releases the core and counts down in global CPU ticks.

Programs are generated internally, rather than typed by the user. Supported instructions are PRINT, DECLARE, ADD, SUBTRACT, SLEEP, and FOR. Each process owns its uint16 variables; missing variables begin at zero and arithmetic clamps to 0-65,535. PRINT can include a variable using `+name`; default generated messages are `Hello world from <name>!`. Logs include a timestamp and the simulated core number. Each process retains its most recent 1,000 PRINT logs to limit memory use.

FOR stores an instruction block and repeat count, with at most three nesting levels. The process keeps its place in loops when it sleeps or loses its time slice. Instruction progress counts the ordinary operations executed after repetition; FOR headers do not consume a separate tick. Generation preserves the configured instruction count including repeated operations. Large counts use compact loop blocks instead of allocating the expanded program.

Reports show current occupied cores divided by total cores, rather than a lifetime average. Running, ready/sleeping, and finished processes are listed separately so waiting processes remain visible. Each report copies scheduler/process state together. Finished process details and PRINT logs remain available for reporting; process variables and instruction contents are cleared at completion.

## Before assessment

Build and check the program before the quiz. During the assessment, follow the requirement to change only `config.txt` and run the already-built application from the project folder:

```powershell
.\build\ProcessScheduler.exe
```

Do not use `run.ps1` during the quiz because it recompiles. Prepare the required video evidence and technical presentation covering command recognition, console UI, command interpretation, process representation, and scheduling.
