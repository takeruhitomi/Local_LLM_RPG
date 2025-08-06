@echo off
echo Downloading KH Dot Font...
echo.
echo This script will download the KH Dot Font package from the official source.
echo The font is licensed under SIL Open Font License 1.1.
echo.

if not exist "fonts" mkdir fonts
cd fonts

echo Downloading font package from http://jikasei.me/font/kh-dotfont/
echo Please visit the following URL to manually download the font:
echo.
echo http://jikasei.me/font/kh-dotfont/
echo.
echo Instructions:
echo 1. Download khdotfont-20150527.zip from the above URL
echo 2. Extract the zip file
echo 3. Copy KH-Dot-Hibiya-24.ttf from the extracted folder
echo 4. Rename it to ipaexg.ttf
echo 5. Place it in the fonts/ directory
echo.
echo The font is licensed under SIL Open Font License 1.1
echo Commercial use is permitted under this license.
echo.

pause
