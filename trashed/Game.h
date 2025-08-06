// Game.h (ステータス・装備システム実装版)

#ifndef GAME_H
#define GAME_H

#include <string>
#include <vector>
#include <memory>
#include <future>
#include <map>
#include "LlmManager.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

struct SDL_Texture_Deleter { void operator()(SDL_Texture* tex) const; };
using TexturePtr = std::unique_ptr<SDL_Texture, SDL_Texture_Deleter>;

// ★★★ 追加: ステータスを管理する構造体 ★★★
struct Stats {
    int hp = 0, mp = 0, atk = 0, def = 0, mat = 0, mdf = 0, spd = 0;
};

// ★★★ 追加: アイテム情報を管理する構造体 ★★★
enum class ItemType { WEAPON, ARMOR, USABLE, KEY };
struct Item {
    std::string name;
    ItemType type;
    Stats stats;
    bool is_equipped = false;
};

class Game {
public:
    Game(const std::map<std::string, std::string>& model_paths);
    ~Game();

    bool init();
    void run();

private:
    enum class GameState { TITLE, STORY, CONVERSATION, PROCESSING_GM, PROCESSING_NPC };

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    TTF_Font* titleFont = nullptr;
    TTF_Font* uiFont = nullptr;
    // ★★★ 追加: ステータス表示用の小さいフォント ★★★
    TTF_Font* smallFont = nullptr;

    GameState currentState = GameState::TITLE;
    bool quit = false;
    
    std::string basePath;
    std::map<std::string, std::string> modelPaths;

    TexturePtr titleBgTexture;
    TexturePtr villageBgTexture;
    TexturePtr villageElderTexture = nullptr;

    std::string inputText = "";
    std::vector<std::string> conversationLog;
    
    // ★★★ 修正: プレイヤーの所持品をItem構造体のベクターに変更 ★★★
    std::vector<Item> playerInventory;
    // ★★★ 追加: アイテムデータベース ★★★
    std::map<std::string, Item> itemDatabase;

    // ★★★ 追加: プレイヤーステータス関連 ★★★
    Stats playerBaseStats;
    Stats playerCurrentStats;
    Item* equippedWeapon = nullptr;
    Item* equippedArmor = nullptr;

    std::vector<std::string> introStory;
    int currentStoryIndex = 0;

    std::unique_ptr<LlmManager> llmManager;
    
    std::future<GmResponse> gm_future;
    std::future<std::string> npc_future;
    GmResponse gm_response_buffer;

    Uint32 lastKeypressTime = 0;
    const Uint32 keypressDelay = 250; 

    Uint8 npcImageAlpha = 0;
    bool isNpcImageVisible = false;
    const int fadeSpeed = 4;

    void handleEvents();
    void update();
    void render();
    
    void render_Title();
    void render_Field();

    bool loadResources();
    void cleanup();
    void pushToLog(const std::string& text);
    void renderUI();
    // ★★★ 追加: ステータスパネル描画用の関数 ★★★
    void renderStatusPanel();
    TexturePtr renderText(const std::string &text, TTF_Font* font, SDL_Color color);

    // ★★★ 追加: アイテムとステータスを管理するヘルパー関数 ★★★
    void initializeDatabase();
    void onInventoryClick(int item_index);
    void recalculateStats();
};

#endif // GAME_H
