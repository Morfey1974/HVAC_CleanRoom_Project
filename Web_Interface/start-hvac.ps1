# HVAC CleanRoom — one-click start: DB (docker) + API + web UI, then open the browser.
# Not 'Stop': in Windows PowerShell docker warnings on stderr would abort the script.
$ErrorActionPreference = 'Continue'
trap {
    Write-Host "Ошибка: $_" -ForegroundColor Red
    Read-Host 'Нажмите Enter для выхода'
    exit 1
}
$root = $PSScriptRoot
$apiUrl = 'http://localhost:5181/api/health'
$webUrl = 'http://localhost:5180'
$Host.UI.RawUI.WindowTitle = 'HVAC CleanRoom — запуск'

function Test-Url($url) {
    try { (Invoke-WebRequest $url -UseBasicParsing -TimeoutSec 2).StatusCode -eq 200 } catch { $false }
}

function Wait-Until($what, [scriptblock]$check, $seconds) {
    Write-Host "Ожидание: $what..." -NoNewline
    for ($i = 0; $i -lt $seconds; $i++) {
        if (& $check) { Write-Host ' готово' -ForegroundColor Green; return }
        Start-Sleep 1
        Write-Host '.' -NoNewline
    }
    Write-Host ' не дождались' -ForegroundColor Red
    Read-Host 'Нажмите Enter для выхода'
    exit 1
}

# 1. Docker Desktop
docker info *> $null
if ($LASTEXITCODE -ne 0) {
    $dd = Join-Path $env:ProgramFiles 'Docker\Docker\Docker Desktop.exe'
    if (Test-Path $dd) { Start-Process $dd }
    Wait-Until 'Docker' { docker info *> $null; $LASTEXITCODE -eq 0 } 120
}

# 2. Database
Push-Location $root
docker compose up -d db | Out-Null
Pop-Location
Wait-Until 'база данных' { (docker inspect -f '{{.State.Health.Status}}' hvac-db 2>$null) -eq 'healthy' } 60

# 3. API
if (-not (Test-Url $apiUrl)) {
    Start-Process powershell -WindowStyle Minimized -WorkingDirectory (Join-Path $root 'HvacConfigurator.Api') `
        -ArgumentList '-NoExit', '-Command', "`$Host.UI.RawUI.WindowTitle='HVAC API'; dotnet run"
    Wait-Until 'сервер' { Test-Url $apiUrl } 120
}

# 4. Web UI
if (-not (Test-Url $webUrl)) {
    $web = Join-Path $root 'hvac-web'
    if (-not (Test-Path (Join-Path $web 'node_modules'))) {
        Push-Location $web; npm install; Pop-Location
    }
    Start-Process powershell -WindowStyle Minimized -WorkingDirectory $web `
        -ArgumentList '-NoExit', '-Command', "`$Host.UI.RawUI.WindowTitle='HVAC Web'; npm run dev"
    Wait-Until 'веб-интерфейс' { Test-Url $webUrl } 60
}

Start-Process $webUrl
