// main.cpp - Prompt Quest entry point
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
        // Llama3.1ベースの日本語チューニングモデル Llama-3.1-8B-EZO-1.1-it
        std::map<std::string, std::string> model_paths = {
            {"GM", "llama.cpp/models/Llama-3.1-8B-EZO-1.1-it.i1-Q4_K_M.gguf"},     
            {"NPC", "llama.cpp/models/Llama-3.1-8B-EZO-1.1-it.i1-Q4_K_M.gguf"},  
            {"BATTLE", "llama.cpp/models/Llama-3.1-8B-EZO-1.1-it.i1-Q4_K_M.gguf"}
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
