# GitHub Copilot — Basic Instructions

Purpose: provide minimal repository-specific guidance for Copilot and contributors on how to build this project on Windows using PowerShell.

Build (Windows PowerShell):

Run the following in PowerShell to ensure the correct MSYS2/ucrt64 toolchain is on the path and to build the project:

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;" + $env:PATH
cmake --build build -j4 2>&1
Write-Host "Exit: $LASTEXITCODE"
```

Notes:
- Use PowerShell (not cmd) when running the commands above.
- The command prepends `C:\msys64\ucrt64\bin` so the correct compiler/toolchain from MSYS2/UCRT64 is used.
- If you prefer to configure and generate the build files manually, run the appropriate `cmake` configure command first.
