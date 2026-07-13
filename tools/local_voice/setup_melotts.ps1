param(
    [switch]$SkipWarmup
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$ThirdPartyDir = Join-Path $ProjectRoot "ThirdParty"
$InstallerDir = Join-Path $ThirdPartyDir "Installers"
$PythonDir = Join-Path $ThirdPartyDir "Python310"
$PythonExe = Join-Path $PythonDir "python.exe"
$VenvDir = Join-Path $ThirdPartyDir "MeloTTS\.venv310"
$VenvPython = Join-Path $VenvDir "Scripts\python.exe"
$CacheDir = Join-Path $ThirdPartyDir "MeloTTS\hf-cache"
$InstallerPath = Join-Path $InstallerDir "python-3.10.11-amd64.exe"
$InstallerUrl = "https://www.python.org/ftp/python/3.10.11/python-3.10.11-amd64.exe"

New-Item -ItemType Directory -Force $InstallerDir | Out-Null
New-Item -ItemType Directory -Force $CacheDir | Out-Null

if (!(Test-Path $PythonExe)) {
    if (!(Test-Path $InstallerPath)) {
        Write-Host "Downloading Python 3.10.11..."
        Invoke-WebRequest -Uri $InstallerUrl -OutFile $InstallerPath
    }

    Write-Host "Installing Python 3.10.11 into $PythonDir..."
    Start-Process -FilePath $InstallerPath -ArgumentList @(
        "/quiet",
        "InstallAllUsers=0",
        "TargetDir=`"$PythonDir`"",
        "Include_launcher=0",
        "PrependPath=0",
        "Include_pip=1",
        "Include_test=0",
        "Shortcuts=0"
    ) -Wait -NoNewWindow
}

Write-Host "Using Python:"
& $PythonExe --version

if (!(Test-Path $VenvPython)) {
    Write-Host "Creating MeloTTS virtual environment..."
    & $PythonExe -m venv $VenvDir
}

Write-Host "Installing MeloTTS dependencies. This can take several minutes..."
& $VenvPython -m pip install --upgrade pip "setuptools<81" wheel
& $VenvPython -m pip install git+https://github.com/myshell-ai/MeloTTS.git
& $VenvPython -m pip install eunjeon "setuptools<81"
& $VenvPython -m pip uninstall -y unidic

$env:HF_HOME = $CacheDir
$env:TRANSFORMERS_CACHE = $CacheDir
$env:HUGGINGFACE_HUB_CACHE = Join-Path $CacheDir "hub"
$env:HF_HUB_DISABLE_SYMLINKS_WARNING = "1"

Write-Host "Checking MeloTTS import..."
& $VenvPython -c "from melo.api import TTS; print('MeloTTS import OK')"

if (!$SkipWarmup) {
    $WarmupOutputDir = Join-Path $ProjectRoot "Saved\LocalVoice\TTS"
    $WarmupOutput = Join-Path $WarmupOutputDir "melotts_setup_check.wav"
    New-Item -ItemType Directory -Force $WarmupOutputDir | Out-Null
    Write-Host "Generating Korean warmup voice. The first run downloads model files..."
    & $VenvPython (Join-Path $PSScriptRoot "melotts_speak.py") `
        --text "싸우자고? 좋아. 대신 후회하지 마." `
        --output $WarmupOutput `
        --language kr `
        --speed 1.0 `
        --cache-dir $CacheDir
    Write-Host "Warmup WAV created: $WarmupOutput"
}

Write-Host ""
Write-Host "MeloTTS setup complete."
Write-Host "Game config expects:"
Write-Host "  LocalTTSExecutablePath=ThirdParty/MeloTTS/.venv310/Scripts/python.exe"
Write-Host "  LocalTTSScriptPath=tools/local_voice/melotts_speak.py"
