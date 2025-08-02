// LLM.h
// llama.cppとの連携を担当するクラスのヘッダーファイル

#ifndef LLM_H
#define LLM_H

#include <string>
#include <vector>
#include "llama.h"

class LLM {
public:
    LLM();
    ~LLM();

    bool init(const std::string& model_path);
    std::string generateResponse(const std::string& prompt);
    void cleanup();

private:
    llama_model* model = nullptr;
    llama_context* ctx = nullptr;
    llama_model_params model_params;
    llama_context_params ctx_params;
};

#endif // LLM_H