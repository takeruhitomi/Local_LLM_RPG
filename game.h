// Game.h
// ゲーム全体のロジックを管理するクラスのヘッダーファイル

#ifndef GAME_H
#define GAME_H

#include <string>
#include <vector>
#include <memory>
#include "LLM.h"

// SDLのヘッダーをインクルードして、型定義を解決
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>


// --- ヘルパー構造体 ---
// SDL_Textureを安全に解放するためのカスタムデリータ
struct SDL_Texture_Deleter { void operator()(SDL_Texture* tex) const; };
// TexturePtrというエイリアスを定義
using TexturePtr = std::unique_ptr<SDL_Texture, SDL_Texture_Deleter>;

class Game {
public:
    Game(const std::string& model_path);
    ~Game();

    bool init();
    void run();

private:
    // ゲームの状態を定義
    enum class GameState { TITLE, STORY, CONVERSATION, PROCESSING_LLM };

    // SDL関連
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    TTF_Font* titleFont = nullptr;
    TTF_Font* uiFont = nullptr;

    // ゲーム状態
    GameState currentState = GameState::TITLE;
    bool quit = false;
    
    // リソースパス
    std::string basePath;
    std::string modelPath;

    // リソース
    TexturePtr titleBgTexture;
    TexturePtr villageBgTexture;

    // テキストとUI
    std::string inputText = "";
    std::vector<std::string> conversationLog;
    
    // ストーリー
    std::vector<std::string> introStory;
    int currentStoryIndex = 0;

    // LLM
    LLM llm;

    // メインループの構成要素
    void handleEvents();
    void update();
    void render();

    // シーンごとのイベント処理
    void handleEvents_Title();
    void handleEvents_Story();
    void handleEvents_Conversation();
    
    // シーンごとの描画処理
    void render_Title();
    void render_Field();

    // ヘルパー関数
    bool loadResources(); // ★★★ 戻り値の型を void から bool に修正 ★★★
    void cleanup();
    void pushToLog(const std::string& text);
    void renderUI();
    TexturePtr renderText(const std::string &text, TTF_Font* font, SDL_Color color);
};

#endif // GAME_H
