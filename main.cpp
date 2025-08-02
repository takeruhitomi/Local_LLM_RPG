// main.cpp
// ゲームを起動するエントリーポイント

#include "Game.h"
// WinMainを使用するためにWindows.hをインクルード
#if defined(_WIN32)
#include <Windows.h>
#endif

// Windowsアプリケーションの標準的なエントリーポイント
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // モデルへのパスを渡す
    std::string model_path = "llama.cpp/models/ELYZA-japanese-Llama-2-7b-instruct-q8_0.gguf";
    
    Game game(model_path);
    if (game.init()) {
        game.run();
    }
    
    return 0;
}
