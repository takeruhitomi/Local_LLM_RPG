// LlmManager.h (プロンプト改善版)

#ifndef LLM_MANAGER_H
#define LLM_MANAGER_H

#include <string>
#include <vector>
#include <memory>
#include <map>
#include "llama.h"

struct ChatMessage {
    std::string role;
    std::string content;
};

struct GmResponse {
    std::string scene_context; 
    std::string action = "CONTINUE";
    std::vector<std::string> items;
};

class LlmManager {
public:
    LlmManager(const std::map<std::string, std::string>& model_paths);
    ~LlmManager();

    LlmManager(const LlmManager&) = delete;
    LlmManager& operator=(const LlmManager&) = delete;

    GmResponse generateGmResponse(const std::vector<ChatMessage>& history);
    std::string generateNpcDialogue(const std::vector<ChatMessage>& history, const std::string& scene_context);

private:
    struct LlmInstance {
        llama_model* model = nullptr;
        llama_context* ctx = nullptr;
    };

    std::map<std::string, LlmInstance> instances;

    std::string run_inference(const std::string& role, const std::string& prompt);
    
    GmResponse parseGmResponse(const std::string& json_str);
};

#endif // LLM_MANAGER_H
