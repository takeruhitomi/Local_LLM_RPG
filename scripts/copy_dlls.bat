@echo off
REM copy_dlls.bat - MSYS2環境でのDLL自動コピースクリプト（Windows版）

echo 必要なDLLをbuildディレクトリにコピーしています...

REM buildディレクトリに移動
cd /d "%~dp0..\build" || (
    echo Error: buildディレクトリが見つかりません
    pause
    exit /b 1
)

REM SDL2関連DLL
copy "C:\msys64\mingw64\bin\SDL2.dll" . >nul 2>&1 || echo Warning: SDL2.dll not found
copy "C:\msys64\mingw64\bin\SDL2_image.dll" . >nul 2>&1 || echo Warning: SDL2_image.dll not found
copy "C:\msys64\mingw64\bin\SDL2_ttf.dll" . >nul 2>&1 || echo Warning: SDL2_ttf.dll not found

REM MinGW関連DLL
copy "C:\msys64\mingw64\bin\libgcc_s_seh-1.dll" . >nul 2>&1 || echo Warning: libgcc_s_seh-1.dll not found
copy "C:\msys64\mingw64\bin\libstdc++-6.dll" . >nul 2>&1 || echo Warning: libstdc++-6.dll not found
copy "C:\msys64\mingw64\bin\libwinpthread-1.dll" . >nul 2>&1 || echo Warning: libwinpthread-1.dll not found

REM 画像形式サポート用DLL（オプション）
copy "C:\msys64\mingw64\bin\libpng16-16.dll" . >nul 2>&1 || echo Info: libpng16-16.dll not found (optional)
copy "C:\msys64\mingw64\bin\libjpeg-8.dll" . >nul 2>&1 || echo Info: libjpeg-8.dll not found (optional)
copy "C:\msys64\mingw64\bin\zlib1.dll" . >nul 2>&1 || echo Info: zlib1.dll not found (optional)

echo DLLコピー完了。game.exeを実行できます。
pause
