#!/bin/bash
# copy_dlls.sh - MSYS2環境でのDLL自動コピースクリプト

echo "必要なDLLをbuildディレクトリにコピーしています..."

# buildディレクトリに移動
cd build || exit 1

# SDL2関連DLL
cp /mingw64/bin/SDL2.dll ./ 2>/dev/null || echo "Warning: SDL2.dll not found"
cp /mingw64/bin/SDL2_image.dll ./ 2>/dev/null || echo "Warning: SDL2_image.dll not found"
cp /mingw64/bin/SDL2_ttf.dll ./ 2>/dev/null || echo "Warning: SDL2_ttf.dll not found"

# MinGW関連DLL
cp /mingw64/bin/libgcc_s_seh-1.dll ./ 2>/dev/null || echo "Warning: libgcc_s_seh-1.dll not found"
cp /mingw64/bin/libstdc++-6.dll ./ 2>/dev/null || echo "Warning: libstdc++-6.dll not found"
cp /mingw64/bin/libwinpthread-1.dll ./ 2>/dev/null || echo "Warning: libwinpthread-1.dll not found"

# 画像形式サポート用DLL（オプション）
cp /mingw64/bin/libpng16-16.dll ./ 2>/dev/null || echo "Info: libpng16-16.dll not found (optional)"
cp /mingw64/bin/libjpeg-8.dll ./ 2>/dev/null || echo "Info: libjpeg-8.dll not found (optional)"
cp /mingw64/bin/zlib1.dll ./ 2>/dev/null || echo "Info: zlib1.dll not found (optional)"

echo "DLLコピー完了。game.exeを実行できます。"
