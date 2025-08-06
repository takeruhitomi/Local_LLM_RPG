# Prompt Quest（仮） - ローカルLLM RPG

![Prompt Quest](https://img.shields.io/badge/Game-Prompt%20Quest-blue)
![Language](https://img.shields.io/badge/Language-C%2B%2B-blue)
![LLM](https://img.shields.io/badge/LLM-Llama%203.1%208B-green)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)

**Prompt Quest（仮）** は、あなたの言葉が冒険を形作る革新的なRPG。SLM（小規模言語モデル）を使用して、会話、戦闘、物語の選択が自然言語入力によって駆動され、よりダイナミックで、より自由なゲーム体験を。

## ゲーム機能

- **動的ストーリーテリング**: プレイヤーの選択に応答するAI駆動ナラティブ
- **自然言語戦闘**: 自分の言葉で攻撃を描写
- **知的NPC**: AI駆動キャラクターとの深い会話
- **ローカルAI処理**: インターネット不要 - すべてのAIがマシン上で動作

## ストーリー概要

かつて「調和のクリスタル」に祝福された世界で、「静寂」と呼ばれる謎の災厄がすべての色と生命を存在から奪い始めます。あなたは「始まりの村」で冒険を開始し、クリスタルの力を復活させる古代の知識を持つ賢い長老に導かれます。

## 技術仕様

- **C++ゲームエンジン**: グラフィックと入力にSDL2を使用
- **llama.cpp統合**: 効率的なローカルLLM推論
- **マルチロールAIシステム**: 
  - **GM**: ストーリー進行のゲームマスター
  - **NPC**: 自然な対話生成  
  - **BATTLE**: 戦闘解決とダメージ計算

## 前提条件

- **オペレーティングシステム**: Windows 10/11
- **メモリ**: 最低8GB RAM（16GB推奨）
- **ストレージ**: 約10GBの空き容量
- **CPU**: 強ければ強いほど良い
- **開発ツール**: 
  - MSYS2、Visual Studio
  - CMake 4.0.3

## 🚀 インストール手順

### 1. リポジトリのクローン
```bash
git clone https://github.com/takeruhitomi/Local_LLM_RPG.git
cd Local_LLM_RPG
```

### 2. llama.cppのセットアップ
```bash
git clone https://github.com/ggerganov/llama.cpp.git
cd llama.cpp
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

### 3. LLMモデルのダウンロード

**方法A: 手動ダウンロード**
1. モデルファイルをダウンロード: `Llama-3.1-8B-EZO-1.1-it.i1-Q4_K_M.gguf`
2. ディレクトリを作成: `llama.cpp/models/`  
3. モデルファイルを配置: `llama.cpp/models/Llama-3.1-8B-EZO-1.1-it.i1-Q4_K_M.gguf`

**方法B: ダウンロードスクリプトを使用**
```bash
# ダウンロードスクリプトを実行
./scripts/download_model.bat
```

**モデル情報:**
- **モデル**: Llama-3.1-8B-EZO-1.1-it（日本語チューニング済み）
- **形式**: GGUF Q4_K_M量子化
- **サイズ**: 約4.6GB
- **ソース**: Hugging Faceモデルリポジトリ

### 4. 日本語フォントのダウンロード

1. アクセス: http://jikasei.me/font/kh-dotfont/
2. `khdotfont-20150527.zip`をダウンロード
3. zipファイルを展開
4. 展開されたフォルダから`KH-Dot-Hibiya-24.ttf`をコピー
5. `fonts/`ディレクトリを作成してファイルを配置

**フォント情報:**
- **フォント**: KH Dot Font - Hibiya 24pt
- **ライセンス**: SIL Open Font License 1.1（商用利用可能）
- **ソース**: http://jikasei.me/font/kh-dotfont/
- **必要ファイル**: `fonts/KH-Dot-Hibiya-24.ttf`

### 5. 依存関係のインストール

**SDL2ライブラリ**（Windows）:
```bash
# SDL2、SDL2_image、SDL2_ttf開発ライブラリをダウンロード
# ローカルディレクトリに展開し、環境変数を設定
```

### 6. ゲームのビルド
```bash
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

### 7. ゲームの実行
```bash
./game.exe
```

## プレイ方法

1. **開始**: タイトル画面でENTERキーを押す
2. **会話**: NPCとの会話を入力
3. **戦闘**: 攻撃を創造的に記述（例：「火の魔法で攻撃」）
4. **戦略**: 敵の弱点を突いて追加ダメージを与える

### 戦闘システム
- **ダメージ計算**: 記述と敵のステータスに基づく
- **クリティカルヒット**: 創造的な弱点攻撃でダメージ増

## 📁 プロジェクト構造

```
Local_LLM_RPG/
├── main.cpp              # アプリケーション エントリポイント
├── Game.h/.cpp           # メインゲームエンジン
├── LlmManager.h/.cpp     # LLM統合レイヤー
├── CMakeLists.txt        # ビルド設定
├── fonts/                # ゲームフォント
├── images/               # ゲームアートワーク
│   ├── background/       # 背景画像
│   ├── npcs/            # NPCポートレート
│   └── monsters/        # モンスターアートワーク
├── llama.cpp/           # LLM推論エンジン（gitサブモジュール）
└── build/               # ビルド出力ディレクトリ
```

## 設定

### モデルパス
`main.cpp`を編集してモデルファイルパスを変更:
```cpp
std::map<std::string, std::string> model_paths = {
    {"GM", "llama.cpp/models/your-model.gguf"},
    {"NPC", "llama.cpp/models/your-model.gguf"},
    {"BATTLE", "llama.cpp/models/your-model.gguf"}
};
```

## ライセンス

このプロジェクトはMITライセンスの下でライセンスされています - 詳細は[LICENSE](LICENSE)ファイルを参照してください。