// main.cpp (2モデル構成版)
#include "Game.h"
#include <stdexcept>
#include <iostream>
#include <map>

#if defined(_WIN32)
#include <Windows.h>
#endif

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
#endif

    try {
        // ★★★ 修正: 役割ごとにモデルパスを指定 ★★★
        // ここでは例として、両方に同じモデルを指定していますが、
        // GM用とNPC用に別々のモデルを割り当てることも可能です。
        std::map<std::string, std::string> model_paths = {
            {"GM", "llama.cpp/models/ggml-model-Q8_0.gguf"},
            {"NPC", "llama.cpp/models/ggml-model-Q8_0.gguf"}
        };

        Game game(model_paths);
        if (game.init()) {
            game.run();
        }
    } catch (const std::exception& e) {
        std::cerr << "A fatal error occurred: " << e.what() << std::endl;
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", e.what(), NULL);
        return 1;
    }
    
    return 0;
}
