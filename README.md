# Prompt Quest - Local LLM RPG

![Prompt Quest](https://img.shields.io/badge/Game-Prompt%20Quest-blue)
![Language](https://img.shields.io/badge/Language-C%2B%2B-blue)
![LLM](https://img.shields.io/badge/LLM-Llama%203.1%208B-green)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)

**Prompt Quest** is an innovative RPG where your words shape the adventure. Using advanced local LLM (Large Language Model) technology, every conversation, battle, and story decision is driven by natural language input, creating a truly dynamic and personalized gaming experience.

## 🎮 Game Features

- **Dynamic Storytelling**: AI-powered narrative that responds to your choices
- **Natural Language Combat**: Describe your attacks in your own words
- **Intelligent NPCs**: Deep conversations with AI-driven characters
- **Japanese Fantasy Setting**: Experience the world of "Silent" corruption
- **Local AI Processing**: No internet required - all AI runs on your machine

## 🎯 Game Story

In a world once blessed by the "Harmony Crystal," a mysterious calamity called "Silence" begins to drain all color and life from existence. You start your journey in the "Village of Beginnings," guided by a wise elder who holds ancient knowledge about restoring the crystal's power.

## 🛠️ Technical Features

- **C++ Game Engine**: Built with SDL2 for graphics and input
- **llama.cpp Integration**: Efficient local LLM inference
- **Multi-Role AI System**: 
  - **GM**: Game master for story progression
  - **NPC**: Natural dialogue generation  
  - **BATTLE**: Combat resolution and damage calculation
- **Memory Optimization**: Smart model instance sharing
- **Japanese Language Support**: Full UTF-8 support for Japanese text

## 📋 Prerequisites

- **Operating System**: Windows 10/11
- **Memory**: At least 8GB RAM (16GB recommended)
- **Storage**: ~10GB free space
- **CPU**: Modern multi-core processor
- **Development Tools**: 
  - MinGW-w64 or Visual Studio
  - CMake 3.16+
  - Git

## 🚀 Installation

### 1. Clone the Repository
```bash
git clone https://github.com/takeruhitomi/Local_LLM_RPG.git
cd Local_LLM_RPG
```

### 2. Setup llama.cpp
```bash
git clone https://github.com/ggerganov/llama.cpp.git
cd llama.cpp
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
mingw32-make -j8
cd ../..
```

### 3. Download the LLM Model

**Option A: Manual Download**
1. Download the model file: `Llama-3.1-8B-EZO-1.1-it.i1-Q4_K_M.gguf`
2. Create directory: `llama.cpp/models/`  
3. Place the model file in: `llama.cpp/models/Llama-3.1-8B-EZO-1.1-it.i1-Q4_K_M.gguf`

**Option B: Use Download Script**
```bash
# Run the download script (if available)
./scripts/download_model.bat
```

**Model Information:**
- **Model**: Llama-3.1-8B-EZO-1.1-it (Japanese tuned)
- **Format**: GGUF Q4_K_M quantization
- **Size**: ~4.6GB
- **Source**: Hugging Face model repository

### 4. Download Japanese Font

**Option A: Manual Download**
1. Visit: http://jikasei.me/font/kh-dotfont/
2. Download `khdotfont-20150527.zip`
3. Extract the zip file
4. Copy `KH-Dot-Hibiya-24.ttf` from the extracted folder
5. Rename it to `ipaexg.ttf`
6. Create `fonts/` directory and place the file there

**Option B: Use Download Script**
```bash
# Run the font download helper script
./scripts/download_fonts.bat
```

**Font Information:**
- **Font**: KH Dot Font - Hibiya 24pt
- **License**: SIL Open Font License 1.1 (Commercial use permitted)
- **Source**: http://jikasei.me/font/kh-dotfont/
- **Required File**: `fonts/ipaexg.ttf`

### 5. Install Dependencies

**SDL2 Libraries** (Windows):
```bash
# Download SDL2, SDL2_image, SDL2_ttf development libraries
# Extract to a local directory and set environment variables
```

### 6. Build the Game
```bash
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
mingw32-make
```

### 7. Run the Game
```bash
./game.exe
```

## 🎮 How to Play

1. **Start**: Press ENTER on the title screen
2. **Conversation**: Type naturally to talk with the village elder
3. **Exploration**: Describe your actions when given choices
4. **Combat**: Use creative descriptions for attacks (e.g., "attack with fire magic")
5. **Strategy**: Exploit enemy weaknesses for extra damage

### Combat System
- **Forest Guardian**: Weak to fire attacks
- **Damage Calculation**: Based on your description and enemy stats
- **Critical Hits**: Creative weakness exploitation deals 1.5-2x damage

## 📁 Project Structure

```
Local_LLM_RPG/
├── main.cpp              # Application entry point
├── Game.h/.cpp           # Main game engine
├── LlmManager.h/.cpp     # LLM integration layer
├── CMakeLists.txt        # Build configuration
├── fonts/                # Game fonts (Japanese support)
├── images/               # Game artwork
│   ├── background/       # Background images
│   ├── npcs/            # NPC portraits
│   └── monsters/        # Monster artwork
├── llama.cpp/           # LLM inference engine (git submodule)
└── build/               # Build output directory
```

## 🔧 Configuration

### Model Paths
Edit `main.cpp` to change model file paths:
```cpp
std::map<std::string, std::string> model_paths = {
    {"GM", "llama.cpp/models/your-model.gguf"},
    {"NPC", "llama.cpp/models/your-model.gguf"},
    {"BATTLE", "llama.cpp/models/your-model.gguf"}
};
```

### Performance Tuning
- **Thread Count**: Adjust in `LlmManager.cpp` (`n_threads` parameter)
- **Context Size**: Modify `n_ctx` for longer conversations  
- **Batch Size**: Change `n_batch` for memory optimization

## 🤝 Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for:
- New game features
- Performance improvements
- Additional language support
- Bug fixes

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- **llama.cpp**: For efficient local LLM inference
- **SDL2**: For graphics and input handling
- **Llama 3.1 EZO**: For the Japanese-tuned language model
- **ggerganov**: For the amazing llama.cpp project

## 📞 Support

If you encounter issues:
1. Check the [Issues](https://github.com/takeruhitomi/Local_LLM_RPG/issues) page
2. Verify your model file is correctly placed
3. Ensure all dependencies are installed
4. Check system requirements

---

**Prompt Quest** - Where your words become reality! 🎭✨
