#include "Game.h"
#include <SDL_image.h>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <algorithm>

const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;

void SDL_Texture_Deleter::operator()(SDL_Texture* tex) const { if (tex) SDL_DestroyTexture(tex); }

Game::Game(const std::map<std::string, std::string>& model_paths) : modelPaths(model_paths) {
    introStory = {
        "かつて、世界は万物の調和を司る「調和のクリスタル」の恩恵を受け、平和と繁栄を謳歌していた。",
        "しかし、ある日、どこからともなく現れた謎の災厄「静寂」が世界を覆い始める。",
        "物語は、世界の片隅にある「始まりの村」から始まる...",
    };
    transitionStory = {
        "長老の言葉を胸に、あなたは村の門をくぐった。",
        "一歩外に出ると、空気は重く、色彩は褪せている。これが「静寂」の影響か...",
        "村のすぐそばに広がる「静寂の森」。不気味な静けさの中、あなたは意を決して足を踏み入れた。",
        "...",
        "森の奥、ひときわ大きな木の前で、異様な気配があなたを捉える！"
    };
}

Game::~Game() { cleanup(); }

bool Game::init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) { std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl; return false; }
    if (!(IMG_Init(IMG_INIT_JPG) & IMG_INIT_JPG)) { std::cerr << "IMG_Init Error: " << IMG_GetError() << std::endl; return false; }
    if (TTF_Init() == -1) { std::cerr << "TTF_Init Error: " << TTF_GetError() << std::endl; return false; }

    window = SDL_CreateWindow("Prompt Quest", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) { std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl; return false; }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) { std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl; return false; }

    char* path = SDL_GetBasePath();
    if (path) {
        basePath = path;
        SDL_free(path);
        for (char& c : basePath) { if (c == '\\') c = '/'; }
        size_t pos = basePath.find("build/");
        if (pos!= std::string::npos) basePath = basePath.substr(0, pos);
    } else {
        basePath = "./";
    }
    
    std::map<std::string, std::string> full_model_paths;
    for(const auto& pair : modelPaths) {
        full_model_paths[pair.first] = basePath + pair.second;
    }

    initializeDatabase();
    
    if (!loadResources()) { std::cerr << "Failed to load resources." << std::endl; return false; }

    playerBaseStats = {50, 20, 10, 8, 5, 5, 7};
    recalculateStats();

    try {
        llmManager = std::make_unique<LlmManager>(full_model_paths);
    } catch (const std::exception& e) {
        std::cerr << "Fatal LLM Error: " << e.what() << std::endl;
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "LLM Load Error", e.what(), window);
        return false;
    }

    SDL_StartTextInput();
    
    return true;
}

void Game::initializeDatabase() {
    itemDatabase["初心者の剣"] = {"初心者の剣", ItemType::WEAPON, {0, 0, 5, 0, 0, 0, 0}};
    itemDatabase["革の鎧"] = {"革の鎧", ItemType::ARMOR, {0, 0, 0, 5, 0, 0, 0}};
    
    // モンスターに弱点情報を追加
    Monster forestGuardian;
    forestGuardian.name = "森の守護者";
    forestGuardian.stats = {150, 0, 25, 15, 10, 10, 8};
    forestGuardian.weaknesses = {"火", "炎", "燃焼", "火属性", "ファイア", "火魔法"};
    forestGuardian.description = "古い森の精霊が「静寂」に侵された姿。木の身体を持つため極端に火に弱い。";
    
    // std::moveでMonsterをマップに移動
    monsterDatabase["森の守護者"] = std::move(forestGuardian);
}

bool Game::loadResources() {
    std::string fontPath = basePath + "fonts/KH-Dot-Hibiya-24.ttf";
    titleFont = TTF_OpenFont(fontPath.c_str(), 48);
    uiFont = TTF_OpenFont(fontPath.c_str(), 24);
    smallFont = TTF_OpenFont(fontPath.c_str(), 18);
    if (!titleFont || !uiFont || !smallFont) { 
        std::cerr << "Failed to load font: " << TTF_GetError() << std::endl; 
        return false; 
    }

    auto load_texture = [&](TexturePtr& tex, const std::string& file, Uint8 r, Uint8 g, Uint8 b) {
        std::string fullPath = basePath + file;
        std::cout << "Loading texture: " << fullPath << std::endl; // デバッグ用ログ追加
        SDL_Surface* surf = IMG_Load(fullPath.c_str());
        if (!surf) {
            std::cerr << "Failed to load image: " << file << " - " << IMG_GetError() << std::endl;
            return false;
        }
        SDL_SetColorKey(surf, SDL_TRUE, SDL_MapRGB(surf->format, r, g, b));
        tex = TexturePtr(SDL_CreateTextureFromSurface(renderer, surf));
        if (!tex) {
            std::cerr << "Failed to create texture for: " << file << " - " << SDL_GetError() << std::endl;
        }
        SDL_FreeSurface(surf);
        return (tex != nullptr);
    };

    if (!load_texture(titleBgTexture, "images/background/spring.jpg", 0, 0, 0)) return false;
    if (!load_texture(villageBgTexture, "images/background/start_village.jpg", 0, 0, 0)) return false;
    if (!load_texture(villageElderTexture, "images/npcs/village_elder.jpg", 255, 255, 255)) return false;
    if (!load_texture(forestBgTexture, "images/background/forest.jpg", 0, 0, 0)) return false;
    
    TexturePtr guardianTexture;
    if (!load_texture(guardianTexture, "images/monsters/forest_guardian.jpg", 255, 255, 255)) {
        std::cerr << "Failed to load forest guardian texture" << std::endl;
        return false;
    }
    
    // データベース内のモンスターにテクスチャを設定
    if (monsterDatabase.find("森の守護者") != monsterDatabase.end()) {
        monsterDatabase["森の守護者"].texture = std::move(guardianTexture);
        std::cout << "Successfully loaded forest guardian texture" << std::endl;
    } else {
        std::cerr << "Monster '森の守護者' not found in database" << std::endl;
        return false;
    }

    return true;
}

void Game::run() {
    while (!quit) {
        handleEvents();
        update();
        render();
        SDL_Delay(16);
    }
}

void Game::handleEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) quit = true;

        if (e.type == SDL_MOUSEBUTTONDOWN) {
            if (e.button.button == SDL_BUTTON_LEFT) {
                int mouseX, mouseY;
                SDL_GetMouseState(&mouseX, &mouseY);
                
                if (showDepartureButton) {
                    SDL_Rect buttonRect = { 50, SCREEN_HEIGHT - 250 - 50, 200, 40 };
                    if (mouseX >= buttonRect.x && mouseX <= buttonRect.x + buttonRect.w &&
                        mouseY >= buttonRect.y && mouseY <= buttonRect.y + buttonRect.h)
                    {
                        currentState = GameState::TRANSITION_TO_FOREST;
                        showDepartureButton = false;
                        isNpcImageVisible = false;

                        isForestBgVisible = false;
                        isMonsterVisible = false;
                        forestBgAlpha = 0;
                        monsterAlpha = 0;
                        
                        conversationLog.clear();
                        pushToLog(transitionStory[0]);
                        lastTransitionTime = SDL_GetTicks();
                        return;
                    }
                }

                SDL_Rect itemPanelRect = { 50 + 900 + 20, SCREEN_HEIGHT - 250, SCREEN_WIDTH - (50 + 900 + 20) - 50, 220 };
                if (mouseX >= itemPanelRect.x && mouseX <= itemPanelRect.x + itemPanelRect.w &&
                    mouseY >= itemPanelRect.y && mouseY <= itemPanelRect.y + itemPanelRect.h)
                {
                    int itemTopY = itemPanelRect.y + 40;
                    int itemHeight = 23;
                    int clickedIndex = (mouseY - itemTopY) / itemHeight;

                    if (clickedIndex >= 0 && clickedIndex < playerInventory.size()) {
                        onInventoryClick(clickedIndex);
                    }
                }
            }
        }

        if ((currentState == GameState::CONVERSATION || currentState == GameState::BATTLE) && e.type == SDL_TEXTINPUT) {
            inputText += e.text.text;
        }

        if (e.type == SDL_KEYDOWN) {
            Uint32 currentTime = SDL_GetTicks();
            if ((currentState == GameState::CONVERSATION || currentState == GameState::BATTLE) && e.key.keysym.sym == SDLK_BACKSPACE && !inputText.empty()) {
                inputText.pop_back();
                lastKeypressTime = currentTime - (keypressDelay - 100);
                continue;
            }
            if (currentTime < lastKeypressTime + keypressDelay) continue;

            switch (currentState) {
                case GameState::TITLE:
                    if (e.key.keysym.sym == SDLK_RETURN) {
                        currentState = GameState::STORY;
                        pushToLog(introStory[currentStoryIndex]);
                        lastStoryTime = SDL_GetTicks();
                        lastKeypressTime = currentTime;
                    }
                    break;
                case GameState::STORY:
                    break;
                case GameState::CONVERSATION:
                    if (e.key.keysym.sym == SDLK_RETURN && !inputText.empty()) {
                        pushToLog("> " + inputText);
                        inputText = "";
                        currentState = GameState::PROCESSING_GM;
                        lastKeypressTime = currentTime;
                    }
                    break;
                case GameState::BATTLE:
                    if (e.key.keysym.sym == SDLK_RETURN && !inputText.empty()) {
                        pushToLog("> " + inputText);
                        inputText = "";
                        currentState = GameState::PROCESSING_BATTLE;
                        lastKeypressTime = currentTime;
                    }
                    break;
                default:
                    break;
            }
        }
    }
}

void Game::update() {
    if (isNpcImageVisible) {
        if (npcImageAlpha < 255) {
            int tempAlpha = npcImageAlpha + fadeSpeed;
            npcImageAlpha = std::min(255, tempAlpha);
        }
    } else {
        if (npcImageAlpha > 0) {
            int tempAlpha = npcImageAlpha - fadeSpeed;
            npcImageAlpha = std::max(0, tempAlpha);
        }
    }

    // 森の背景フェード処理
    if (isForestBgVisible) {
        if (forestBgAlpha < 255) {
            int tempAlpha = forestBgAlpha + fadeSpeed;
            forestBgAlpha = std::min(255, tempAlpha);
        }
    } else {
        if (forestBgAlpha > 0) {
            int tempAlpha = forestBgAlpha - fadeSpeed;
            forestBgAlpha = std::max(0, tempAlpha);
        }
    }

    // モンスター画像フェード処理
    if (isMonsterVisible) {
        if (monsterAlpha < 255) {
            int tempAlpha = monsterAlpha + fadeSpeed;
            monsterAlpha = std::min(255, tempAlpha);
        }
    } else {
        if (monsterAlpha > 0) {
            int tempAlpha = monsterAlpha - fadeSpeed;
            monsterAlpha = std::max(0, tempAlpha);
        }
    }

    if (currentState == GameState::STORY) {
        if (SDL_GetTicks() > lastStoryTime + 2500) {
            currentStoryIndex++;
            if (currentStoryIndex < introStory.size()) {
                pushToLog(introStory[currentStoryIndex]);
                lastStoryTime = SDL_GetTicks();
            } else {
                currentState = GameState::CONVERSATION;
                isNpcImageVisible = true;
                pushToLog("長老: あなたか...。よく来てくれた。話したいことがある。");
            }
        }
        return;
    }

    if (currentState == GameState::TRANSITION_TO_FOREST) {
        if (SDL_GetTicks() > lastTransitionTime + 2500) {
            currentTransitionIndex++;
            if (currentTransitionIndex < transitionStory.size()) {
                pushToLog(transitionStory[currentTransitionIndex]);
                lastTransitionTime = SDL_GetTicks();
                
                if (currentTransitionIndex == 1) {
                    // "一歩外に出ると..." の時点で森の背景を表示開始
                    isForestBgVisible = true;
                } else if (currentTransitionIndex == 2) {
                    // "村のすぐそばに広がる「静寂の森」..." の時点でNPC画像を非表示
                    isNpcImageVisible = false;
                }
            } else {
                currentState = GameState::BATTLE;
                currentEnemyTemplate = &monsterDatabase["森の守護者"];
                if (!currentEnemyTemplate->texture) {
                    std::cerr << "Error: Enemy texture is null." << std::endl;
                }
                currentEnemyStats = currentEnemyTemplate->stats;
                conversationLog.clear();
                pushToLog(currentEnemyTemplate->name + " が現れた！");
                pushToLog("コマンドを入力して戦おう");
                
                // モンスター画像を表示開始
                isMonsterVisible = true;
            }
        }
        return;
    }

    // GM応答
    if (currentState == GameState::PROCESSING_GM && !gm_future.valid()) {
        std::vector<ChatMessage> history;
        history.push_back({"system", ""});
        for(const auto& log : conversationLog) {
            if (log.rfind("> ", 0) == 0) {
                history.push_back({"user", log.substr(2)});
            } else if (log.rfind("長老: ", 0) == 0) {
                history.push_back({"assistant", log.substr(strlen("長老: "))});
            }
        }
        gm_future = std::async(std::launch::async, &LlmManager::generateGmResponse, llmManager.get(), history);
    }

    // GM応答の完了チェック
    if (currentState == GameState::PROCESSING_GM && gm_future.valid()) {
        if (gm_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try {
                gm_response_buffer = gm_future.get();
                gm_future = {}; 
                if (gm_response_buffer.action == "DEPART") {
                    showDepartureButton = true;
                }
                currentState = GameState::PROCESSING_NPC;
            } catch (const std::exception& e) {
                std::cerr << "GM thread exception: " << e.what() << std::endl;
                pushToLog("長老: （考えがまとまらぬ...）");
                currentState = GameState::CONVERSATION;
                gm_future = {};
            }
        }
    }

    if (currentState == GameState::PROCESSING_NPC && !npc_future.valid()) {
        std::vector<ChatMessage> history_for_npc;
        history_for_npc.push_back({"system", ""});
        int count = 0;
        for (auto it = conversationLog.rbegin(); it != conversationLog.rend() && count < 4; ++it) {
             if (it->rfind("> ", 0) == 0) {
                history_for_npc.insert(history_for_npc.begin() + 1, {"user", it->substr(2)});
                count++;
            } else if (it->rfind("長老: ", 0) == 0) {
                history_for_npc.insert(history_for_npc.begin() + 1, {"assistant", it->substr(strlen("長老: "))});
                count++;
            }
        }
        npc_future = std::async(std::launch::async, &LlmManager::generateNpcDialogue, llmManager.get(), history_for_npc, gm_response_buffer.scene_context);
    }

    if (currentState == GameState::PROCESSING_NPC && npc_future.valid()) {
        if (npc_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try {
                std::string dialogue = npc_future.get();
                if (!dialogue.empty()) {
                    pushToLog("長老: " + dialogue);
                }
                if (gm_response_buffer.action == "DEPART") {
                    for (const auto& item_name : gm_response_buffer.items) {
                        if (itemDatabase.count(item_name)) {
                            playerInventory.push_back(itemDatabase[item_name]);
                            pushToLog("（" + item_name + " を手に入れた！）");
                        }
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "NPC thread exception: " << e.what() << std::endl;
                pushToLog("長老: （...むずかしいことを言うのう）");
            }
            currentState = GameState::CONVERSATION;
            npc_future = {};
        }
    }

    // 戦闘応答
    if (currentState == GameState::PROCESSING_BATTLE && !battle_future.valid()) {
        // 最後のプレイヤー行動を取得
        std::string last_action;
        for (auto it = conversationLog.rbegin(); it != conversationLog.rend(); ++it) {
            if (it->rfind("> ", 0) == 0) {
                last_action = it->substr(2);
                break;
            }
        }
        
        auto stats_to_string = [](const Stats& s) {
            return "HP:" + std::to_string(s.hp) + ", ATK:" + std::to_string(s.atk) + ", DEF:" + std::to_string(s.def);
        };

        // 敵の弱点情報を含む詳細情報を作成
        std::string enemy_info = currentEnemyTemplate->description + " 弱点: ";
        for (size_t i = 0; i < currentEnemyTemplate->weaknesses.size(); ++i) {
            enemy_info += currentEnemyTemplate->weaknesses[i];
            if (i < currentEnemyTemplate->weaknesses.size() - 1) {
                enemy_info += "、";
            }
        }

        battle_future = std::async(std::launch::async, &LlmManager::generateBattleResponse, llmManager.get(), 
            stats_to_string(playerCurrentStats), stats_to_string(currentEnemyStats), last_action, enemy_info);
    }

    // 戦闘応答の完了チェック
    if (currentState == GameState::PROCESSING_BATTLE && battle_future.valid()) {
        if (battle_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try {
                BattleResponse res = battle_future.get();
                pushToLog(res.effect_text);
                if (res.hit) {
                    currentEnemyStats.hp -= res.damage;
                    pushToLog(currentEnemyTemplate->name + "に" + std::to_string(res.damage) + "のダメージ！");
                    if (currentEnemyStats.hp <= 0) {
                        currentEnemyStats.hp = 0;
                        pushToLog(currentEnemyTemplate->name + "を倒した！");
                        currentState = GameState::CONVERSATION;
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Battle thread exception: " << e.what() << std::endl;
                pushToLog("（しかし何も起こらなかった...）");
            }

            if (currentEnemyStats.hp > 0) {
                 pushToLog(currentEnemyTemplate->name + "の攻撃！");
                 int damage_to_player = std::max(0, currentEnemyStats.atk - playerCurrentStats.def);
                 playerCurrentStats.hp -= damage_to_player;
                 pushToLog("プレイヤーは" + std::to_string(damage_to_player) + "のダメージを受けた！");
                 if(playerCurrentStats.hp <= 0) {
                    playerCurrentStats.hp = 0;
                    pushToLog("あなたは倒れてしまった...");
                    resetGame();
                 } else {
                    currentState = GameState::BATTLE;
                 }
            }
            battle_future = {};
        }
    }
}

void Game::render() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    if (currentState == GameState::TITLE) {
        render_Title();
    } else if (currentState == GameState::BATTLE || currentState == GameState::PROCESSING_BATTLE) {
        render_Battle();
    } else {
        render_Field();
    }
    SDL_RenderPresent(renderer);
}

void Game::render_Title() {
    SDL_RenderCopy(renderer, titleBgTexture.get(), NULL, NULL);
    
    SDL_Color titleColor = { 255, 215, 0, 255 };  // ゴールド色
    SDL_Color subtitleColor = { 200, 200, 255, 255 };  // ライトブルー
    SDL_Color instructionColor = { 255, 255, 255, 255 };  // 白色
    
    TexturePtr titleTex = renderText("Prompt Quest", titleFont, titleColor);
    int w, h;
    SDL_QueryTexture(titleTex.get(), NULL, NULL, &w, &h);
    SDL_Rect dst = {(SCREEN_WIDTH - w)/2, 80, w, h};
    SDL_RenderCopy(renderer, titleTex.get(), NULL, &dst);
    
    TexturePtr subtitleTex = renderText("～ Words Shape Your Adventure ～", uiFont, subtitleColor);
    SDL_QueryTexture(subtitleTex.get(), NULL, NULL, &w, &h);
    dst = {(SCREEN_WIDTH - w)/2, 140, w, h};
    SDL_RenderCopy(renderer, subtitleTex.get(), NULL, &dst);
    
    TexturePtr msgTex = renderText("Press ENTER to Start", uiFont, instructionColor);
    SDL_QueryTexture(msgTex.get(), NULL, NULL, &w, &h);
    dst = {(SCREEN_WIDTH - w)/2, 600, w, h};
    SDL_RenderCopy(renderer, msgTex.get(), NULL, &dst);
}

void Game::render_Field() {
    if (currentState == GameState::TRANSITION_TO_FOREST && isForestBgVisible && forestBgAlpha > 0) {
        // 森の背景を半透明で表示
        SDL_SetTextureBlendMode(forestBgTexture.get(), SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(forestBgTexture.get(), forestBgAlpha);
        SDL_RenderCopy(renderer, forestBgTexture.get(), NULL, NULL);
        
        // 村の背景を徐々に薄くする
        int villageAlpha = 255 - forestBgAlpha;
        SDL_SetTextureBlendMode(villageBgTexture.get(), SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(villageBgTexture.get(), villageAlpha);
        SDL_RenderCopy(renderer, villageBgTexture.get(), NULL, NULL);
    } else {
        // 通常時は村の背景
        SDL_RenderCopy(renderer, villageBgTexture.get(), NULL, NULL);
    }

    if (npcImageAlpha > 0 && villageElderTexture) {
        SDL_SetTextureBlendMode(villageElderTexture.get(), SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(villageElderTexture.get(), npcImageAlpha);
        
        int w, h;
        SDL_QueryTexture(villageElderTexture.get(), NULL, NULL, &w, &h);
        float scale = (SCREEN_HEIGHT * 0.75f) / h;
        int disp_w = static_cast<int>(w * scale);
        int disp_h = static_cast<int>(h * scale);

        SDL_Rect dstRect = { SCREEN_WIDTH - disp_w - 60, (SCREEN_HEIGHT - disp_h) / 2, disp_w, disp_h };
        SDL_RenderCopy(renderer, villageElderTexture.get(), NULL, &dstRect);
    }

    renderUI();
    renderStatusPanel();
}

void Game::render_Battle() {
    SDL_RenderCopy(renderer, forestBgTexture.get(), NULL, NULL);

    if (currentEnemyTemplate && isMonsterVisible && monsterAlpha > 0) {
        if (currentEnemyTemplate->texture) {
            SDL_SetTextureBlendMode(currentEnemyTemplate->texture.get(), SDL_BLENDMODE_BLEND);
            SDL_SetTextureAlphaMod(currentEnemyTemplate->texture.get(), monsterAlpha);
            
            int w, h;
            SDL_QueryTexture(currentEnemyTemplate->texture.get(), NULL, NULL, &w, &h);
            float scale = (SCREEN_HEIGHT * 0.6f) / h;
            int disp_w = static_cast<int>(w * scale);
            int disp_h = static_cast<int>(h * scale);
            SDL_Rect dstRect = { (SCREEN_WIDTH - disp_w) / 2, (SCREEN_HEIGHT - disp_h) / 2 - 50, disp_w, disp_h };
            SDL_RenderCopy(renderer, currentEnemyTemplate->texture.get(), NULL, &dstRect);
        } else {
            std::cerr << "Error: Enemy texture is null." << std::endl;
        }
    } else if (currentEnemyTemplate) {
        std::cerr << "Monster alpha is 0." << std::endl;
    } else {
        std::cerr << "Error: currentEnemyTemplate is null." << std::endl;
    }

    renderUI();
    renderStatusPanel();
    renderEnemyStatusPanel();
}


void Game::renderUI() {
    SDL_Rect mainPanelRect = { 50, SCREEN_HEIGHT - 250, 900, 220 };
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 192);
    SDL_RenderFillRect(renderer, &mainPanelRect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &mainPanelRect);

    if (showDepartureButton) {
        SDL_Rect buttonRect = { mainPanelRect.x, mainPanelRect.y - 50, 200, 40 };
        SDL_SetRenderDrawColor(renderer, 30, 80, 150, 220);
        SDL_RenderFillRect(renderer, &buttonRect);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &buttonRect);
        TexturePtr btnText = renderText("静寂の森へ旅立つ", uiFont, {255, 255, 255, 255});
        if (btnText) {
            int w, h;
            SDL_QueryTexture(btnText.get(), NULL, NULL, &w, &h);
            SDL_Rect dst = { buttonRect.x + (buttonRect.w - w) / 2, buttonRect.y + (buttonRect.h - h) / 2, w, h };
            SDL_RenderCopy(renderer, btnText.get(), NULL, &dst);
        }
    }

    int logY = mainPanelRect.y + mainPanelRect.h - 50;
    int logX = mainPanelRect.x + 10;
    int logW = mainPanelRect.w - 20;
    SDL_Color textColor = { 255, 255, 255, 255 };
    
    for (int i = conversationLog.size() - 1; i >= 0; --i) {
        if (logY < mainPanelRect.y) break; 
        SDL_Surface* surface = TTF_RenderUTF8_Blended_Wrapped(uiFont, conversationLog[i].c_str(), textColor, logW);
        if(surface) {
            logY -= surface->h;
            SDL_Rect dst = {logX, logY, surface->w, surface->h};
            if (dst.y < mainPanelRect.y) { SDL_FreeSurface(surface); break; }
            TexturePtr texture(SDL_CreateTextureFromSurface(renderer, surface));
            SDL_RenderCopy(renderer, texture.get(), NULL, &dst);
            logY -= 5;
            SDL_FreeSurface(surface);
        }
    }

    SDL_Rect inputRect = { mainPanelRect.x, mainPanelRect.y + mainPanelRect.h - 40, mainPanelRect.w, 40 };
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_RenderFillRect(renderer, &inputRect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &inputRect);

    std::string displayText = "> ";
    if (currentState == GameState::PROCESSING_GM || currentState == GameState::PROCESSING_NPC || currentState == GameState::PROCESSING_BATTLE) {
        displayText += "考えている...";
    } else if (currentState == GameState::CONVERSATION || currentState == GameState::BATTLE) {
        displayText += inputText;
        if (SDL_GetTicks() / 500 % 2) displayText += "_";
    }
    
    TexturePtr inputTex = renderText(displayText, uiFont, textColor);
    if (inputTex) {
        int w, h;
        SDL_QueryTexture(inputTex.get(), NULL, NULL, &w, &h);
        SDL_Rect dst = { inputRect.x + 10, inputRect.y + 5, w, h };
        SDL_RenderCopy(renderer, inputTex.get(), NULL, &dst);
    }
    
    SDL_Rect itemPanelRect = { mainPanelRect.x + mainPanelRect.w + 20, mainPanelRect.y, SCREEN_WIDTH - (mainPanelRect.x + mainPanelRect.w + 20) - 50, mainPanelRect.h };
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 192);
    SDL_RenderFillRect(renderer, &itemPanelRect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &itemPanelRect);
    
    TexturePtr itemTitle = renderText("もちもの", uiFont, textColor);
    if(itemTitle) {
        int w, h;
        SDL_QueryTexture(itemTitle.get(), NULL, NULL, &w, &h);
        SDL_Rect dst = {itemPanelRect.x + 10, itemPanelRect.y + 10, w, h};
        SDL_RenderCopy(renderer, itemTitle.get(), NULL, &dst);
    }

    int itemY = itemPanelRect.y + 40;
    for(const auto& item : playerInventory) {
        std::string itemText = item.name;
        TexturePtr itemTex = renderText(itemText, smallFont, textColor);
        if(itemTex) {
            int w, h;
            SDL_QueryTexture(itemTex.get(), NULL, NULL, &w, &h);
            SDL_Rect dst = {itemPanelRect.x + 20, itemY, w, h};
            SDL_RenderCopy(renderer, itemTex.get(), NULL, &dst);

            if (item.is_equipped) {
                SDL_Color equippedColor = {255, 220, 0, 255};
                TexturePtr equippedMark = renderText("E", smallFont, equippedColor);
                if (equippedMark) {
                    int markW, markH;
                    SDL_QueryTexture(equippedMark.get(), NULL, NULL, &markW, &markH);
                    SDL_Rect markDst = {itemPanelRect.x + itemPanelRect.w - markW - 10, itemY, markW, markH};
                    SDL_RenderCopy(renderer, equippedMark.get(), NULL, &markDst);
                }
            }
            itemY += h + 5;
        }
    }
}

void Game::renderStatusPanel() {
    SDL_Rect statusPanelRect = { SCREEN_WIDTH - 250, 20, 230, 200 };
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 192);
    SDL_RenderFillRect(renderer, &statusPanelRect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &statusPanelRect);

    SDL_Color textColor = { 255, 255, 255, 255 };
    int currentY = statusPanelRect.y + 10;
    
    auto renderStat = [&](const std::string& name, int value) {
        std::string text = name + " : " + std::to_string(value);
        TexturePtr tex = renderText(text, smallFont, textColor);
        if (tex) {
            int w, h;
            SDL_QueryTexture(tex.get(), NULL, NULL, &w, &h);
            SDL_Rect dst = { statusPanelRect.x + 15, currentY, w, h };
            SDL_RenderCopy(renderer, tex.get(), NULL, &dst);
            currentY += h + 5;
        }
    };

    renderStat("HP", playerCurrentStats.hp);
    renderStat("MP", playerCurrentStats.mp);
    renderStat("ATK", playerCurrentStats.atk);
    renderStat("DEF", playerCurrentStats.def);
    renderStat("MAT", playerCurrentStats.mat);
    renderStat("MDF", playerCurrentStats.mdf);
    renderStat("SPD", playerCurrentStats.spd);
}

void Game::renderEnemyStatusPanel() {
    if (!currentEnemyTemplate) return;

    SDL_Rect panelRect = { 20, 20, 230, 80 };
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 150, 0, 0, 192);
    SDL_RenderFillRect(renderer, &panelRect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &panelRect);

    SDL_Color textColor = { 255, 255, 255, 255 };
    int currentY = panelRect.y + 10;

    TexturePtr nameTex = renderText(currentEnemyTemplate->name, uiFont, textColor);
    if (nameTex) {
        int w, h;
        SDL_QueryTexture(nameTex.get(), NULL, NULL, &w, &h);
        SDL_Rect dst = { panelRect.x + 15, currentY, w, h };
        SDL_RenderCopy(renderer, nameTex.get(), NULL, &dst);
        currentY += h + 8;
    }

    std::string hpText = "HP : " + std::to_string(currentEnemyStats.hp);
    TexturePtr hpTex = renderText(hpText, smallFont, textColor);
    if (hpTex) {
        int w, h;
        SDL_QueryTexture(hpTex.get(), NULL, NULL, &w, &h);
        SDL_Rect dst = { panelRect.x + 15, currentY, w, h };
        SDL_RenderCopy(renderer, hpTex.get(), NULL, &dst);
    }
}


void Game::pushToLog(const std::string& text) { 
    conversationLog.push_back(text); 
    if (conversationLog.size() > 20) {
        conversationLog.erase(conversationLog.begin());
    }
}

TexturePtr Game::renderText(const std::string &text, TTF_Font* font, SDL_Color color) {
    if (text.empty()) return nullptr;
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface) return nullptr;
    TexturePtr texture(SDL_CreateTextureFromSurface(renderer, surface));
    SDL_FreeSurface(surface);
    return texture;
}

void Game::cleanup() {
    if (gm_future.valid()) gm_future.wait();
    if (npc_future.valid()) npc_future.wait();
    if (battle_future.valid()) battle_future.wait();
    llmManager.reset();
    SDL_StopTextInput();
    if(titleFont) TTF_CloseFont(titleFont);
    if(uiFont) TTF_CloseFont(uiFont);
    if(smallFont) TTF_CloseFont(smallFont);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

void Game::onInventoryClick(int item_index) {
    if (item_index < 0 || item_index >= playerInventory.size()) return;

    Item& clickedItem = playerInventory[item_index];

    if (clickedItem.type == ItemType::WEAPON) {
        if (clickedItem.is_equipped) {
            clickedItem.is_equipped = false;
            equippedWeapon = nullptr;
        } else {
            if (equippedWeapon) {
                equippedWeapon->is_equipped = false;
            }
            clickedItem.is_equipped = true;
            equippedWeapon = &clickedItem;
        }
    } else if (clickedItem.type == ItemType::ARMOR) {
        if (clickedItem.is_equipped) {
            clickedItem.is_equipped = false;
            equippedArmor = nullptr;
        } else {
            if (equippedArmor) {
                equippedArmor->is_equipped = false;
            }
            clickedItem.is_equipped = true;
            equippedArmor = &clickedItem;
        }
    }

    recalculateStats();
}

void Game::recalculateStats() {
    playerCurrentStats = playerBaseStats;
    
    auto addStats = [&](const Stats& itemStats) {
        playerCurrentStats.hp += itemStats.hp;
        playerCurrentStats.mp += itemStats.mp;
        playerCurrentStats.atk += itemStats.atk;
        playerCurrentStats.def += itemStats.def;
        playerCurrentStats.mat += itemStats.mat;
        playerCurrentStats.mdf += itemStats.mdf;
        playerCurrentStats.spd += itemStats.spd;
    };

    if (equippedWeapon) {
        addStats(equippedWeapon->stats);
    }
    if (equippedArmor) {
        addStats(equippedArmor->stats);
    }
}

void Game::resetGame() {
    currentState = GameState::TITLE;
    conversationLog.clear();
    playerInventory.clear();
    
    recalculateStats(); // ステータスを初期値に戻す
    equippedWeapon = nullptr;
    equippedArmor = nullptr;

    isNpcImageVisible = false;
    showDepartureButton = false;
    
    isForestBgVisible = false;
    isMonsterVisible = false;
    forestBgAlpha = 0;
    monsterAlpha = 0;
    
    currentStoryIndex = 0;
    currentTransitionIndex = 0;
}
