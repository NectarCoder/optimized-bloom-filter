#!/usr/bin/env pwsh
<#
Sets up a Python virtual environment in the repository root and installs dependencies from this folder's requirements.txt
#>
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent -Path $MyInvocation.MyCommand.Definition
$RepoRoot = Split-Path -Parent -Path $ScriptDir
$VenvDir = Join-Path $RepoRoot ".venv"
$ReqFile = Join-Path $ScriptDir "requirements.txt"

Write-Host "Setting up Python virtual environment..." -ForegroundColor Green

try {
    $PythonVersion = python --version 2>&1
    Write-Host "Found: $PythonVersion" -ForegroundColor Cyan
} catch {
    Write-Host "Error: Python is not installed or not in PATH" -ForegroundColor Red
    exit 1
}

if (Test-Path $VenvDir) {
    Write-Host "Virtual environment already exists. Skipping creation." -ForegroundColor Yellow
} else {
    python -m venv $VenvDir
    if ($LASTEXITCODE -ne 0) { Write-Host "Error: Failed to create virtual environment" -ForegroundColor Red; exit 1 }
    Write-Host "Virtual environment created successfully" -ForegroundColor Green
}

$ActivateScript = Join-Path $VenvDir "Scripts" "Activate.ps1"
Write-Host "Activating virtual environment..." -ForegroundColor Green
& $ActivateScript

Write-Host "Upgrading pip..." -ForegroundColor Green
python -m pip install --upgrade pip

Write-Host "Installing dependencies from $ReqFile..." -ForegroundColor Green
pip install -r $ReqFile

Write-Host "Setup complete!" -ForegroundColor Green
Write-Host "To activate the virtual environment in the future, run:" -ForegroundColor Cyan
Write-Host "  .\\.venv\\Scripts\\Activate.ps1" -ForegroundColor White
