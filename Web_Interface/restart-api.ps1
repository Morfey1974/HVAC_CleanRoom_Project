# HVAC CleanRoom — restart the API only (after a backend change). Closes the old "HVAC API" window,
# starts a new one, waits for /api/health. The browser is not touched: open pages reconnect by themselves.
$ErrorActionPreference = 'Continue'
$root = $PSScriptRoot
$apiUrl = 'http://localhost:5181/api/health'

function Test-Url($url) {
    try { (Invoke-WebRequest $url -UseBasicParsing -TimeoutSec 2).StatusCode -eq 200 } catch { $false }
}

Get-CimInstance Win32_Process -Filter "Name='powershell.exe' OR Name='pwsh.exe'" |
    Where-Object { $_.CommandLine -like "*WindowTitle='HVAC API'*" } |
    ForEach-Object { taskkill /PID $_.ProcessId /T /F *> $null }
Get-Process HvacConfigurator.Api -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 1

Start-Process powershell -WindowStyle Minimized -WorkingDirectory (Join-Path $root 'HvacConfigurator.Api') `
    -ArgumentList '-NoExit', '-Command', "`$Host.UI.RawUI.WindowTitle='HVAC API'; dotnet run"

for ($i = 0; $i -lt 120; $i++) {
    if (Test-Url $apiUrl) { Write-Host 'API: ready'; exit 0 }
    Start-Sleep 1
}
Write-Host 'API: no answer' -ForegroundColor Red
exit 1
