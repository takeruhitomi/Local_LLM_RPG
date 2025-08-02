// Game.cpp
// ゲーム全体のロジックを管理するクラスの実装ファイル

#include "Game.h"
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <iostream>

// --- 定数 ---
const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;

// --- カスタムデリータの実装 ---
void SDL_Texture_Deleter::operator()(SDL_Texture* tex) const {
    if (tex) {
        SDL_DestroyTexture(tex);
    }
}

// --- Gameクラスの実装 ---
Game::Game(const std::string& model_path) : modelPath(model_path) {
    introStory = {
        "かつて、世界は万物の調和を司る「調和のクリスタル」の恩恵を受け、平和と繁栄を謳歌していた。",
        "しかし、ある日、どこからともなく現れた謎の災厄「静寂（サイレンス）」が世界を覆い始める。",
        "「静寂」は音と色を奪い、生命の活気を無へと還す霧。その中心で、「調和のクリスタル」は砕け散り、その輝く破片は世界の各地へと飛び散ってしまった。",
        "物語は、世界の片隅にある「始まりの村」から始まる...",
        "村の長老「あなたか...。よく来てくれた。話したいことがある。」"
    };
}

Game::~Game() {
    cleanup();
}

bool Game::init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return false;
    }
    if (!(IMG_Init(IMG_INIT_JPG) & IMG_INIT_JPG)) {
        std::cerr << "IMG_Init Error: " << IMG_GetError() << std::endl;
        return false;
    }
    if (TTF_Init() == -1) {
        std::cerr << "TTF_Init Error: " << TTF_GetError() << std::endl;
        return false;
    }

    // ★★★ 実行ファイルの基準パスを取得 ★★★
    char* path = SDL_GetBasePath();
    if (path) {
        basePath = path;
        SDL_free(path);
        // Windowsではパスの区切り文字を'/'に統一すると扱いやすい
        for (char& c : basePath) {
            if (c == '\\') {
                c = '/';
            }
        }
        // build/Debug/ や build/Release/ のような階層を考慮し、プロジェクトルートを指すように調整
        // ここでは単純に build フォルダから実行されることを想定
        size_t pos = basePath.find("build/");
        if (pos != std::string::npos) {
            basePath = basePath.substr(0, pos);
        }

    } else {
        std::cerr << "Could not get base path. Assuming current directory." << std::endl;
        basePath = "./";
    }
    std::cout << "Base Path: " << basePath << std::endl;


    window = SDL_CreateWindow("Project Chimera", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // ★★★ モデルへのパスを基準パスと結合 ★★★
    std::string fullModelPath = basePath + modelPath;
    std::cout << "Loading model from: " << fullModelPath << std::endl;
    if (!llm.init(fullModelPath)) {
        std::cerr << "LLM initialization failed." << std::endl;
        return false;
    }

    if (!loadResources()) {
        std::cerr << "Failed to load resources." << std::endl;
        return false;
    }
    SDL_StartTextInput();
    
    return true;
}

bool Game::loadResources() {
    // ★★★ リソースへのパスを基準パスと結合 ★★★
    std::string titleFontPath = basePath + "fonts/ipaexg.ttf";
    std::string uiFontPath = basePath + "fonts/ipaexg.ttf";
    titleFont = TTF_OpenFont(titleFontPath.c_str(), 48);
    uiFont = TTF_OpenFont(uiFontPath.c_str(), 24);
    if (!titleFont || !uiFont) { 
        std::cerr << "Failed to load font: " << TTF_GetError() << std::endl;
        return false;
    }

    std::string titleBgPath = basePath + "images/background/spring.jpg";
    SDL_Surface* titleBgSurface = IMG_Load(titleBgPath.c_str());
    if(!titleBgSurface) {
        std::cerr << "Failed to load image: " << titleBgPath << " - " << IMG_GetError() << std::endl;
        return false;
    }
    titleBgTexture = TexturePtr(SDL_CreateTextureFromSurface(renderer, titleBgSurface));
    SDL_FreeSurface(titleBgSurface);

    std::string villageBgPath = basePath + "images/background/start_village.jpg";
    SDL_Surface* villageBgSurface = IMG_Load(villageBgPath.c_str());
     if(!villageBgSurface) {
        std::cerr << "Failed to load image: " << villageBgPath << " - " << IMG_GetError() << std::endl;
        return false;
    }
    villageBgTexture = TexturePtr(SDL_CreateTextureFromSurface(renderer, villageBgSurface));
    SDL_FreeSurface(villageBgSurface);

    return true;
}

// run()以下の他の関数は変更なし...
// (以下、前回のGame.cppと同じコードが続きます)
void Game::run() {
    while (!quit) {
        handleEvents();
        update();
        render();
    }
}

void Game::handleEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            quit = true;
        }

        // キー入力は特定の状態でのみ処理
        if (e.type == SDL_KEYDOWN) {
             switch (currentState) {
                case GameState::TITLE:          handleEvents_Title(); break;
                case GameState::STORY:          handleEvents_Story(); break;
                case GameState::CONVERSATION:   handleEvents_Conversation(); break;
                case GameState::PROCESSING_LLM: /* 入力不可 */ break;
            }
        }
        
        // テキスト入力イベントは会話中のみ
        if (currentState == GameState::CONVERSATION && e.type == SDL_TEXTINPUT) {
            inputText += e.text.text;
        }
    }
}

void Game::handleEvents_Title() {
    const Uint8* currentKeyStates = SDL_GetKeyboardState(NULL);
    if (currentKeyStates[SDL_SCANCODE_RETURN]) {
        currentState = GameState::STORY;
        pushToLog(introStory[currentStoryIndex]);
        SDL_Delay(200); // チャタリング防止
    }
}

void Game::handleEvents_Story() {
    const Uint8* currentKeyStates = SDL_GetKeyboardState(NULL);
    if (currentKeyStates[SDL_SCANCODE_RETURN]) {
        currentStoryIndex++;
        if (currentStoryIndex < introStory.size()) {
            pushToLog(introStory[currentStoryIndex]);
        } else {
            currentState = GameState::CONVERSATION;
        }
        SDL_Delay(200); // チャタリング防止
    }
}

void Game::handleEvents_Conversation() {
    const Uint8* currentKeyStates = SDL_GetKeyboardState(NULL);
    if (currentKeyStates[SDL_SCANCODE_BACKSPACE] && inputText.length() > 0) {
        inputText.pop_back();
        SDL_Delay(100); // 連打防止
    }
    else if (currentKeyStates[SDL_SCANCODE_RETURN] && !inputText.empty()) {
        std::string playerInput = inputText;
        pushToLog("> " + playerInput);
        inputText = "";
        
        currentState = GameState::PROCESSING_LLM;
    }
}


void Game::update() {
    // LLMの処理を非同期的に行う
    if (currentState == GameState::PROCESSING_LLM) {
        // conversationLogの最後の要素がプレイヤーの入力
        std::string playerInput = conversationLog.back().substr(2); // "> " を除去
        
        std::string prompt = "あなたは心優しく、世界の異変を憂いている村の長老です。あなたの目の前には、世界を救う使命を帯びた若者がいます。プレイヤーの言葉「" + playerInput + "」に、賢明で導きのある返答をしてください。";
        std::string response = llm.generateResponse(prompt);
        
        // レスポンスが空の場合の処理
        if (response.empty()) {
            response = "（長老は静かに頷いている...）";
        }
        
        pushToLog("長老: " + response);
        currentState = GameState::CONVERSATION;
    }
}

void Game::render() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    if (currentState == GameState::TITLE) {
        render_Title();
    } else {
        render_Field();
    }

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
    renderUI();
}

void Game::renderUI() {
    // メインパネル
    SDL_Rect mainPanelRect = { 50, SCREEN_HEIGHT - 250, 900, 220 };
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 192);
    SDL_RenderFillRect(renderer, &mainPanelRect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &mainPanelRect);

    // 会話ログ
    int logY = mainPanelRect.y + mainPanelRect.h - 80; // 下から描画
    int logX = mainPanelRect.x + 10;
    int logW = mainPanelRect.w - 20;
    SDL_Color textColor = { 255, 255, 255, 255 };
    
    // ログを下から順に描画
    for (int i = conversationLog.size() - 1; i >= 0; --i) {
        if (logY < mainPanelRect.y) break; 
        
        SDL_Surface* surface = TTF_RenderUTF8_Blended_Wrapped(uiFont, conversationLog[i].c_str(), textColor, logW);
        if(surface) {
            logY -= surface->h; // 先にY座標を動かす
            SDL_Rect dst = {logX, logY, surface->w, surface->h};
            if (dst.y < mainPanelRect.y) { // パネル上部にはみ出すなら描画しない
                 SDL_FreeSurface(surface);
                 break;
            }
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_RenderCopy(renderer, texture, NULL, &dst);
            logY -= 5; // 行間
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
    }

    // 入力欄
    SDL_Rect inputRect = { mainPanelRect.x, mainPanelRect.y + mainPanelRect.h - 40, mainPanelRect.w, 40 };
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_RenderFillRect(renderer, &inputRect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &inputRect);

    if (currentState == GameState::CONVERSATION || currentState == GameState::PROCESSING_LLM) {
        std::string displayText = "> " + inputText;
        if (currentState == GameState::CONVERSATION && SDL_GetTicks() / 500 % 2) {
            displayText += "_";
        }
        if (currentState == GameState::PROCESSING_LLM) {
            displayText = "> Generating response...";
        }
        TexturePtr inputTex = renderText(displayText, uiFont, textColor);
        if (inputTex) {
            int w, h;
            SDL_QueryTexture(inputTex.get(), NULL, NULL, &w, &h);
            SDL_Rect dst = { inputRect.x + 10, inputRect.y + 5, w, h };
            SDL_RenderCopy(renderer, inputTex.get(), NULL, &dst);
        }
    }
    
    // 右側パネル
    SDL_Rect equipmentPanelRect = { mainPanelRect.x + mainPanelRect.w + 20, mainPanelRect.y, SCREEN_WIDTH - (mainPanelRect.x + mainPanelRect.w + 20) - 50, 100 };
    SDL_Rect itemPanelRect = { equipmentPanelRect.x, equipmentPanelRect.y + equipmentPanelRect.h + 20, equipmentPanelRect.w, mainPanelRect.h - equipmentPanelRect.h - 20 };
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 192);
    SDL_RenderFillRect(renderer, &equipmentPanelRect);
    SDL_RenderFillRect(renderer, &itemPanelRect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &equipmentPanelRect);
    SDL_RenderDrawRect(renderer, &itemPanelRect);
}


void Game::pushToLog(const std::string& text) {
    conversationLog.push_back(text);
}

TexturePtr Game::renderText(const std::string &text, TTF_Font* font, SDL_Color color) {
    if (text.empty()) return nullptr;
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface) return nullptr;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return TexturePtr(texture);
}

void Game::cleanup() {
    llm.cleanup();
    SDL_StopTextInput();
    if(titleFont) TTF_CloseFont(titleFont);
    if(uiFont) TTF_CloseFont(uiFont);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}
