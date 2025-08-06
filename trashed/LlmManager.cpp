// LlmManager.cpp (プロンプト改善版)

#include "LlmManager.h"
#include <stdexcept>
#include <iostream>
#include <vector>
#include <sstream>
#include <algorithm>

LlmManager::LlmManager(const std::map<std::string, std::string>& model_paths) {
    llama_backend_init();

    for (const auto& pair : model_paths) {
        const std::string& role = pair.first;
        const std::string& path = pair.second;

        LlmInstance instance;
        auto mparams = llama_model_default_params();
        instance.model = llama_model_load_from_file(path.c_str(), mparams);
        if (instance.model == nullptr) {
            throw std::runtime_error("Error: failed to load model for role '" + role + "' from " + path);
        }

        auto cparams = llama_context_default_params();
        cparams.n_ctx = 2048;
        cparams.n_batch = 2048;
        instance.ctx = llama_init_from_model(instance.model, cparams);
        if (instance.ctx == nullptr) {
            llama_model_free(instance.model);
            throw std::runtime_error("Error: failed to create context for role '" + role + "'");
        }
        instances[role] = instance;
        std::cout << "Model for role '" << role << "' loaded successfully." << std::endl;
    }
}

LlmManager::~LlmManager() {
    for (auto& pair : instances) {
        if (pair.second.ctx) llama_free(pair.second.ctx);
        if (pair.second.model) llama_model_free(pair.second.model);
    }
    llama_backend_free();
}

std::string LlmManager::run_inference(const std::string& role, const std::string& prompt) {
    auto it = instances.find(role);
    if (it == instances.end()) {
        return "[ERROR: Role '" + role + "' not found]";
    }
    LlmInstance& instance = it->second;

    const auto* vocab = llama_model_get_vocab(instance.model);
    std::vector<llama_token> tokens_list(prompt.size() + 16);
    int n_tokens = llama_tokenize(vocab, prompt.c_str(), (int)prompt.length(), tokens_list.data(), tokens_list.size(), false, true);
    if (n_tokens < 0) return "[ERROR: Tokenization failed]";
    tokens_list.resize(n_tokens);

    llama_memory_clear(llama_get_memory(instance.ctx), false);
    llama_batch batch = llama_batch_init(n_tokens, 0, 1);
    for (int32_t i = 0; i < n_tokens; ++i) {
        batch.token[i] = tokens_list[i];
        batch.pos[i] = i;
        batch.n_seq_id[i] = 1;
        batch.seq_id[i][0] = 0;
        batch.logits[i] = false;
    }
    batch.n_tokens = n_tokens;
    batch.logits[batch.n_tokens - 1] = true;

    if (llama_decode(instance.ctx, batch) != 0) {
        llama_batch_free(batch);
        return "[ERROR: llama_decode failed]";
    }
    llama_batch_free(batch);

    std::string result_str;
    int n_cur = n_tokens;
    struct llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
    llama_sampler* sampler = llama_sampler_chain_init(sparams);
    llama_sampler_chain_add(sampler, llama_sampler_init_penalties(512, 1.1f, 0.0f, 0.0f));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_k(40));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.95f, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(0.8f));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(1234));
    llama_batch gen_batch = llama_batch_init(1, 0, 1);

    bool has_started_json = (role == "GM");
    int brace_count = 0;

    for (int i = 0; i < 256; ++i) {
        llama_token new_token_id = llama_sampler_sample(sampler, instance.ctx, -1);
        llama_sampler_accept(sampler, new_token_id);

        if (llama_vocab_is_eog(vocab, new_token_id)) break;

        char piece[128] = {0};
        int len = llama_token_to_piece(vocab, new_token_id, piece, sizeof(piece), 0, false);
        if (len < 0) break;
        std::string piece_str(piece, len);

        result_str.append(piece_str);

        if (role == "GM") {
             if (!has_started_json && piece_str.find('{') != std::string::npos) has_started_json = true;
             if (has_started_json) {
                for(char c : piece_str) {
                    if (c == '{') brace_count++;
                    if (c == '}') brace_count--;
                }
                if (brace_count <= 0 && has_started_json) break;
             }
        }
        
        gen_batch.n_tokens = 1;
        gen_batch.token[0] = new_token_id;
        gen_batch.pos[0] = n_cur;
        gen_batch.n_seq_id[0] = 1;
        gen_batch.seq_id[0][0] = 0;
        gen_batch.logits[0] = true;
        if (llama_decode(instance.ctx, gen_batch) != 0) break;
        n_cur++;
    }
    llama_batch_free(gen_batch);
    llama_sampler_free(sampler);
    return result_str;
}

// ★★★ 修正: GMのプロンプトを会話履歴全体を分析するように変更 ★★★
GmResponse LlmManager::generateGmResponse(const std::vector<ChatMessage>& history) {
    std::string system_prompt =
        "あなたはゲームマスターAIです。以下の「会話履歴」全体を分析し、プレイヤーの最後の発言の意図を汲み取ってゲームの状況を判断してください。\n"
        "応答は必ずJSON形式のみを出力してください。JSON以外のテキストは絶対に出力してはいけません。\n\n"
        "```json\n"
        "{\n"
        "  \"scene_context\": \"この後NPCがどのような状況で何をすべきかの場面設定\",\n"
        "  \"action\": \"実行すべきアクション名\",\n"
        "  \"items\": [\"関連アイテム1\", \"関連アイテム2\"]\n"
        "}\n"
        "```\n\n"
        "### アクション名\n"
        "- `CONTINUE`: 会話を続ける。\n"
        "- `GIVE_ITEM`: プレイヤーにアイテムを渡す。\n\n"
        "### 判断基準\n"
        "- プレイヤーが会話の流れの中で「旅に出る」「世界を救う」など、使命を受け入れる意思を明確に示した場合、`action`を`GIVE_ITEM`とし、`items`に`[\"初心者の剣\", \"革の鎧\"]`を指定し、`scene_context`に「若者の覚悟を認め、旅立ちの餞別として装備を渡そうとしている。」と記述してください。\n"
        "- それ以外の会話では、`action`は`CONTINUE`とし、`scene_context`に「若者との会話を続けている。」と記述してください。";

    std::stringstream ss;
    ss << "<|begin_of_text|><|start_header_id|>system<|end_header_id|>\n\n" << system_prompt << "<|eot_id|>";
    
    // GMにも会話履歴全体を渡す
    for (const auto& msg : history) {
        if (msg.role == "system") continue;
        ss << "<|start_header_id|>" << msg.role << "<|end_header_id|>\n\n" << msg.content << "<|eot_id|>";
    }
    ss << "<|start_header_id|>assistant<|end_header_id|>\n\n";

    std::cout << "\n--- Sending to GM Model ---\n" << ss.str() << "\n--------------------------\n";
    std::string raw_response = run_inference("GM", ss.str());
    std::cout << "--- Raw GM Response ---\n" << raw_response << "\n-----------------------\n";
    return parseGmResponse(raw_response);
}

std::string LlmManager::generateNpcDialogue(const std::vector<ChatMessage>& history, const std::string& scene_context) {
    std::string system_prompt =
        "あなたはロールプレイを実行するAIです。以下の「制約条件」「キャラクター設定」「場面設定」を絶対に守り、キャラクターになりきって自然なセリフを一つだけ生成してください。\n"
        "次に続く「会話履歴」を参考に、自然な会話の流れになるように応答を生成してください。\n\n"
        "### 制約条件\n"
        "- あなたの応答は、キャラクター自身のセリフのみでなければなりません。\n"
        "- 地の文、ナレーション、状況説明は絶対に生成してはいけません。\n"
        "- 応答は「」で括らず、セリフそのものだけを出力してください。\n\n"
        "### あなたが演じるキャラクター設定\n"
        "あなたは心優しく、世界の異変を憂いている村の長老です。あなたの目の前には、世界を救う使命を帯びた若者がいます。\n\n"
        "### 今回の場面設定\n"
        + scene_context;

    std::stringstream ss;
    ss << "<|begin_of_text|><|start_header_id|>system<|end_header_id|>\n\n" << system_prompt << "<|eot_id|>";
    
    for (const auto& msg : history) {
        if (msg.role == "system") continue;
        ss << "<|start_header_id|>" << msg.role << "<|end_header_id|>\n\n" << msg.content << "<|eot_id|>";
    }
    ss << "<|start_header_id|>assistant<|end_header_id|>\n\n";
    
    std::cout << "\n--- Sending to NPC Model ---\n" << ss.str() << "\n--------------------------\n";
    std::string raw_response = run_inference("NPC", ss.str());
    std::cout << "--- Raw NPC Response ---\n" << raw_response << "\n-----------------------\n";
    
    size_t pos = raw_response.find("<|eot_id|>");
    if (pos != std::string::npos) {
        raw_response = raw_response.substr(0, pos);
    }
    size_t first = raw_response.find_first_not_of(" \n\r\t");
    if (std::string::npos != first) {
        size_t last = raw_response.find_last_not_of(" \n\r\t");
        return raw_response.substr(first, (last - first + 1));
    }

    return raw_response;
}

GmResponse LlmManager::parseGmResponse(const std::string& raw_str) {
    GmResponse res;
    res.scene_context = "若者との会話を続けている。";

    size_t start_pos = raw_str.find('{');
    size_t end_pos = raw_str.rfind('}');
    if (start_pos == std::string::npos || end_pos == std::string::npos || start_pos >= end_pos) {
        return res;
    }
    std::string json_str = raw_str.substr(start_pos, end_pos - start_pos + 1);

    auto getValue = [&](const std::string& key) -> std::string {
        size_t key_pos = json_str.find("\"" + key + "\"");
        if (key_pos == std::string::npos) return "";
        size_t colon_pos = json_str.find(":", key_pos);
        if (colon_pos == std::string::npos) return "";
        size_t start_quote_pos = json_str.find("\"", colon_pos);
        if (start_quote_pos == std::string::npos) return "";
        size_t end_quote_pos = json_str.find("\"", start_quote_pos + 1);
        if (end_quote_pos == std::string::npos) return "";
        std::string value = json_str.substr(start_quote_pos + 1, end_quote_pos - start_quote_pos - 1);
        return value;
    };
    
    res.scene_context = getValue("scene_context");
    res.action = getValue("action");

    size_t items_key_pos = json_str.find("\"items\"");
    if (items_key_pos != std::string::npos) {
        size_t array_start_pos = json_str.find("[", items_key_pos);
        size_t array_end_pos = json_str.find("]", array_start_pos);
        if (array_start_pos != std::string::npos && array_end_pos != std::string::npos) {
            std::string items_substr = json_str.substr(array_start_pos + 1, array_end_pos - array_start_pos - 1);
            std::stringstream ss(items_substr);
            std::string item;
            while (std::getline(ss, item, ',')) {
                size_t start = item.find("\"");
                size_t end = item.rfind("\"");
                if (start != std::string::npos && end != std::string::npos && start != end) {
                    res.items.push_back(item.substr(start + 1, end - start - 1));
                }
            }
        }
    }

    if (res.action.empty()) res.action = "CONTINUE";
    if (res.scene_context.empty()) res.scene_context = "若者との会話を続けている。";
    return res;
}
