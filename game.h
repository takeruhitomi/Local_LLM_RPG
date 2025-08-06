// Game.h - Prompt Quest: Core game engine implementation

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

struct Stats {
    int hp = 0, mp = 0, atk = 0, def = 0, mat = 0, mdf = 0, spd = 0;
};

enum class ItemType { WEAPON, ARMOR, USABLE, KEY };
struct Item {
    std::string name;
    ItemType type;
    Stats stats;
    bool is_equipped = false;
};

struct Monster {
    std::string name;
    Stats stats;
    TexturePtr texture = nullptr;
    std::vector<std::string> weaknesses;  // 弱点属性リスト（火、氷など）
    std::string description;
    
    // unique_ptrコピー禁止、ムーブのみ許可
    Monster() = default;
    Monster(const Monster&) = delete;
    Monster& operator=(const Monster&) = delete;
    Monster(Monster&&) = default;
    Monster& operator=(Monster&&) = default;
};

class Game {
public:
    Game(const std::map<std::string, std::string>& model_paths);
    ~Game();

    bool init();
    void run();

private:
    enum class GameState { TITLE, STORY, CONVERSATION, PROCESSING_GM, PROCESSING_NPC, TRANSITION_TO_FOREST, BATTLE, PROCESSING_BATTLE };

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    TTF_Font* titleFont = nullptr;
    TTF_Font* uiFont = nullptr;
    TTF_Font* smallFont = nullptr;

    GameState currentState = GameState::TITLE;
    bool quit = false;
    
    std::string basePath;
    std::map<std::string, std::string> modelPaths;

    TexturePtr titleBgTexture;
    TexturePtr villageBgTexture;
    TexturePtr forestBgTexture = nullptr;
    TexturePtr villageElderTexture = nullptr;

    std::string inputText = "";
    std::vector<std::string> conversationLog;
    
    std::vector<Item> playerInventory;
    std::map<std::string, Item> itemDatabase;
    std::map<std::string, Monster> monsterDatabase;

    Stats playerBaseStats;
    Stats playerCurrentStats;
    Item* equippedWeapon = nullptr;
    Item* equippedArmor = nullptr;

    Monster* currentEnemyTemplate = nullptr;
    Stats currentEnemyStats;

    std::vector<std::string> introStory;
    int currentStoryIndex = 0;

    std::unique_ptr<LlmManager> llmManager;
    
    // 非同期処理用
    std::future<GmResponse> gm_future;
    std::future<std::string> npc_future;
    std::future<BattleResponse> battle_future;
    GmResponse gm_response_buffer;

    Uint32 lastKeypressTime = 0;
    const Uint32 keypressDelay = 250; 

    // 画像フェード表示制御
    bool isNpcImageVisible = false;
    bool isForestBgVisible = false;
    bool isMonsterVisible = false;
    Uint8 npcImageAlpha = 0;
    Uint8 forestBgAlpha = 0;
    Uint8 monsterAlpha = 0;
    
    const int fadeSpeed = 8;  // フェードイン・アウト速度
    
    // ストーリー進行制御
    int currentTransitionIndex = 0;
    Uint32 lastTransitionTime = 0;
    Uint32 lastStoryTime = 0;

    bool showDepartureButton = false;
    std::vector<std::string> transitionStory;
    
    void handleEvents();
    void update();
    void render();
    
    void render_Title();
    void render_Field();
    void render_Battle();

    bool loadResources();
    void cleanup();
    void pushToLog(const std::string& text);
    void renderUI();
    void renderStatusPanel();
    void renderEnemyStatusPanel();
    TexturePtr renderText(const std::string &text, TTF_Font* font, SDL_Color color);

    void initializeDatabase();
    void onInventoryClick(int item_index);
    void recalculateStats();
    void resetGame();  // ゲーム状態をタイトル画面に戻す
};

#endif
