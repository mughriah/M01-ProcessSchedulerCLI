M01 PROCESS SCHEDULER AND CLI - CSOPESY GROUP 8

Developers:
  Fred Abenojar
  Mary Faye Caya
  Deo Zamir Diamante
  Gerylyn Guiller

Entry file: main.cpp
Visual Studio project: M01-ProcessSchedulerCLI.vcxproj

Requirements:
  Windows PowerShell and a C++20 compiler.
  Use MinGW-w64 g++ on PATH, or Developer PowerShell for Visual Studio
  with the Desktop development with C++ tools installed.

Build and run from the repository folder:
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\run.ps1

Build only:
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\run.ps1 -BuildOnly

Run regression checks:
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\run.ps1 -Test

Once open, type initialize first. Use screen -s demo to create a process,
process-smi inside its screen, and exit to return to the main menu.

Keep config.txt in the project folder. The script runs from that folder.
After pulling source changes, rebuild using run.ps1. Do not run copied or
old executables. Only a successful build launches the application.

The current config is a short demo setup. Before assessment, build and
verify once. During the quiz, change only config.txt and run the existing
build\ProcessScheduler.exe from the repository folder without rebuilding.

Read README.md for commands, configuration limits, implementation details,
team workflow, and submission preparation.
