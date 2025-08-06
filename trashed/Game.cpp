// Game.cpp (ステータス・装備システム実装版)

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
        "しかし、ある日、どこからともなく現れた謎の災厄「静寂（サイレンス）」が世界を覆い始める。",
        "物語は、世界の片隅にある「始まりの村」から始まる...",
        "村の長老「あなたか...。よく来てくれた。話したいことがある。」"
    };
}

Game::~Game() { cleanup(); }

bool Game::init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) { std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl; return false; }
    if (!(IMG_Init(IMG_INIT_JPG) & IMG_INIT_JPG)) { std::cerr << "IMG_Init Error: " << IMG_GetError() << std::endl; return false; }
    if (TTF_Init() == -1) { std::cerr << "TTF_Init Error: " << TTF_GetError() << std::endl; return false; }

    window = SDL_CreateWindow("Project Chimera", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
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

    if (!loadResources()) { std::cerr << "Failed to load resources." << std::endl; return false; }

    // ★★★ 追加: データベースとステータスの初期化 ★★★
    initializeDatabase();
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

// ★★★ 追加: アイテムの固定ステータスを定義する関数 ★★★
void Game::initializeDatabase() {
    itemDatabase["初心者の剣"] = {"初心者の剣", ItemType::WEAPON, {0, 0, 5, 0, 0, 0, 0}};
    itemDatabase["革の鎧"] = {"革の鎧", ItemType::ARMOR, {0, 0, 0, 5, 0, 0, 0}};
}

bool Game::loadResources() {
    std::string fontPath = basePath + "fonts/ipaexg.ttf";
    titleFont = TTF_OpenFont(fontPath.c_str(), 48);
    uiFont = TTF_OpenFont(fontPath.c_str(), 24);
    // ★★★ 追加: 小さいフォントの読み込み ★★★
    smallFont = TTF_OpenFont(fontPath.c_str(), 18);
    if (!titleFont || !uiFont || !smallFont) { 
        std::cerr << "Failed to load font: " << TTF_GetError() << std::endl; 
        return false; 
    }

    auto load_texture = [&](TexturePtr& tex, const std::string& file, bool transparent_white) {
        SDL_Surface* surf = IMG_Load((basePath + file).c_str());
        if (!surf) {
            std::cerr << "Failed to load image: " << file << " - " << IMG_GetError() << std::endl;
            return false;
        }
        if (transparent_white) {
            SDL_SetColorKey(surf, SDL_TRUE, SDL_MapRGB(surf->format, 255, 255, 255));
        }
        tex = TexturePtr(SDL_CreateTextureFromSurface(renderer, surf));
        SDL_FreeSurface(surf);
        return (tex != nullptr);
    };

    if (!load_texture(titleBgTexture, "images/background/spring.jpg", false)) return false;
    if (!load_texture(villageBgTexture, "images/background/start_village.jpg", false)) return false;
    if (!load_texture(villageElderTexture, "images/npcs/village_elder.jpg", true)) return false;

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

        // ★★★ 追加: マウスクリックイベントの処理 ★★★
        if (e.type == SDL_MOUSEBUTTONDOWN) {
            if (e.button.button == SDL_BUTTON_LEFT) {
                int mouseX, mouseY;
                SDL_GetMouseState(&mouseX, &mouseY);
                
                // もちものパネルの領域を定義
                SDL_Rect itemPanelRect = { 50 + 900 + 20, SCREEN_HEIGHT - 250, SCREEN_WIDTH - (50 + 900 + 20) - 50, 220 };
                if (mouseX >= itemPanelRect.x && mouseX <= itemPanelRect.x + itemPanelRect.w &&
                    mouseY >= itemPanelRect.y && mouseY <= itemPanelRect.y + itemPanelRect.h)
                {
                    // クリックされたアイテムのインデックスを計算
                    int itemTopY = itemPanelRect.y + 40;
                    int itemHeight = 23; // smallFontの高さ + 余白
                    int clickedIndex = (mouseY - itemTopY) / itemHeight;

                    if (clickedIndex >= 0 && clickedIndex < playerInventory.size()) {
                        onInventoryClick(clickedIndex);
                    }
                }
            }
        }

        if (currentState == GameState::CONVERSATION && e.type == SDL_TEXTINPUT) {
            inputText += e.text.text;
        }

        if (e.type == SDL_KEYDOWN) {
            Uint32 currentTime = SDL_GetTicks();
            if (currentState == GameState::CONVERSATION && e.key.keysym.sym == SDLK_BACKSPACE && !inputText.empty()) {
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
                        lastKeypressTime = currentTime;
                    }
                    break;
                case GameState::STORY:
                    if (e.key.keysym.sym == SDLK_RETURN) {
                        currentStoryIndex++;
                        if (currentStoryIndex < introStory.size()) {
                            pushToLog(introStory[currentStoryIndex]);
                        } else {
                            currentState = GameState::CONVERSATION;
                            isNpcImageVisible = true;
                        }
                        lastKeypressTime = currentTime;
                    }
                    break;
                case GameState::CONVERSATION:
                    if (e.key.keysym.sym == SDLK_RETURN && !inputText.empty()) {
                        pushToLog("> " + inputText);
                        inputText = "";
                        currentState = GameState::PROCESSING_GM;
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

    if (currentState == GameState::PROCESSING_GM && gm_future.valid()) {
        if (gm_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try {
                gm_response_buffer = gm_future.get();
                gm_future = {}; 
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
                pushToLog("長老: " + dialogue);

                if (gm_response_buffer.action == "GIVE_ITEM") {
                    for (const auto& item_name : gm_response_buffer.items) {
                        // ★★★ 修正: データベースからアイテム情報を取得してインベントリに追加 ★★★
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
}

void Game::render() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    if (currentState == GameState::TITLE) render_Title();
    else render_Field();
    SDL_RenderPresent(renderer);
}

void Game::render_Title() {
    SDL_RenderCopy(renderer, titleBgTexture.get(), NULL, NULL);
    SDL_Color textColor = { 255, 255, 255, 255 };
    TexturePtr titleTex = renderText("Project Chimera", titleFont, textColor);
    int w, h;
    SDL_QueryTexture(titleTex.get(), NULL, NULL, &w, &h);
    SDL_Rect dst = {(SCREEN_WIDTH - w)/2, 100, w, h};
    SDL_RenderCopy(renderer, titleTex.get(), NULL, &dst);
    TexturePtr msgTex = renderText("Press ENTER to Start", uiFont, textColor);
    SDL_QueryTexture(msgTex.get(), NULL, NULL, &w, &h);
    dst = {(SCREEN_WIDTH - w)/2, 600, w, h};
    SDL_RenderCopy(renderer, msgTex.get(), NULL, &dst);
}

void Game::render_Field() {
    SDL_RenderCopy(renderer, villageBgTexture.get(), NULL, NULL);

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
    // ★★★ 追加: ステータスパネルの描画呼び出し ★★★
    renderStatusPanel();
}

void Game::renderUI() {
    SDL_Rect mainPanelRect = { 50, SCREEN_HEIGHT - 250, 900, 220 };
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 192);
    SDL_RenderFillRect(renderer, &mainPanelRect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &mainPanelRect);

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
    if (currentState == GameState::PROCESSING_GM || currentState == GameState::PROCESSING_NPC) {
        displayText += "考えている...";
    } else if (currentState == GameState::CONVERSATION) {
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
    
    // ★★★ 修正: もちものパネルの描画ロジック ★★★
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

            // 装備マークの描画
            if (item.is_equipped) {
                SDL_Color equippedColor = {255, 220, 0, 255}; // 黄色
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

// ★★★ 追加: ステータスパネルを描画する関数 ★★★
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

// ★★★ 追加: アイテムクリック時の処理 ★★★
void Game::onInventoryClick(int item_index) {
    if (item_index < 0 || item_index >= playerInventory.size()) return;

    Item& clickedItem = playerInventory[item_index];

    if (clickedItem.type == ItemType::WEAPON) {
        if (clickedItem.is_equipped) {
            // 装備を外す
            clickedItem.is_equipped = false;
            equippedWeapon = nullptr;
        } else {
            // 装備する
            if (equippedWeapon) {
                equippedWeapon->is_equipped = false; // 既に何か装備していれば外す
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

// ★★★ 追加: ステータスを再計算する関数 ★★★
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
