@echo off
echo ================================
echo Prompt Quest - Model Download
echo ================================
echo.

:: Create models directory if it doesn't exist
if not exist "llama.cpp\models" (
    echo Creating models directory...
    mkdir "llama.cpp\models"
)

:: Check if model already exists
if exist "llama.cpp\models\Llama-3.1-8B-EZO-1.1-it.i1-Q4_K_M.gguf" (
    echo Model already exists. Skipping download.
    goto :end
)

echo Downloading Llama-3.1-8B-EZO-1.1-it model...
echo Model Size: ~4.6GB - This may take a while depending on your connection.
echo.

:: Try to download using curl (if available)
curl --version >nul 2>&1
if %errorlevel% == 0 (
    echo Using curl to download...
    curl -L -o "llama.cpp\models\Llama-3.1-8B-EZO-1.1-it.i1-Q4_K_M.gguf" ^
    "https://huggingface.co/nitky/Llama-3.1-8B-EZO-1.1-it-GGUF/resolve/main/Llama-3.1-8B-EZO-1.1-it.i1-Q4_K_M.gguf"
    goto :check_download
)

:: Try to download using PowerShell
echo Using PowerShell to download...
powershell -Command "& {Invoke-WebRequest -Uri 'https://huggingface.co/nitky/Llama-3.1-8B-EZO-1.1-it-GGUF/resolve/main/Llama-3.1-8B-EZO-1.1-it.i1-Q4_K_M.gguf' -OutFile 'llama.cpp\models\Llama-3.1-8B-EZO-1.1-it.i1-Q4_K_M.gguf'}"

:check_download
if exist "llama.cpp\models\Llama-3.1-8B-EZO-1.1-it.i1-Q4_K_M.gguf" (
    echo.
    echo ✓ Model downloaded successfully!
    echo File location: llama.cpp\models\Llama-3.1-8B-EZO-1.1-it.i1-Q4_K_M.gguf
) else (
    echo.
    echo ✗ Download failed. Please download manually:
    echo.
    echo 1. Go to: https://huggingface.co/nitky/Llama-3.1-8B-EZO-1.1-it-GGUF
    echo 2. Download: Llama-3.1-8B-EZO-1.1-it.i1-Q4_K_M.gguf
    echo 3. Place it in: llama.cpp\models\
    echo.
)

:end
echo.
echo Press any key to continue...
pause >nul
