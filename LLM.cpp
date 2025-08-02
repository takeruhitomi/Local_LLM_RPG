// LLM.cpp (最新版API対応例)

#include "LLM.h"
#include <iostream>
#include <stdexcept>
#include <vector>

LLM::LLM() {
    llama_backend_init();
}

LLM::~LLM() {
    cleanup();
    llama_backend_free();
}

bool LLM::init(const std::string& model_path) {
    model_params = llama_model_default_params();
    
    // モデル読み込み（新API llama_model_load_from_file）
    model = llama_model_load_from_file(model_path.c_str(), model_params);
    if (model == nullptr) {
        std::cerr << "Failed to load model from " << model_path << std::endl;
        return false;
    }

    ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 2048;
    ctx_params.n_threads = 4;
    ctx_params.n_threads_batch = 4;

    // コンテキスト作成（新API llama_init_from_model）
    ctx = llama_init_from_model(model, ctx_params);
    if (ctx == nullptr) {
        std::cerr << "Failed to create llama_context" << std::endl;
        llama_model_free(model);
        model = nullptr;
        return false;
    }

    std::cout << "LLM model loaded successfully." << std::endl;
    return true;
}

std::string LLM::generateResponse(const std::string& prompt) {
    if (!ctx || !model) return "[ERROR: LLM not initialized]";

    // 語彙取得（const llama_vocab*）
    const llama_vocab *vocab = llama_model_get_vocab(model);

    // KVキャッシュクリア（旧 llama_kv_cache_clear -> llama_kv_self_clear）
    llama_kv_self_clear(ctx);

    // プロンプトをトークン化
    std::vector<llama_token> tokens_list(ctx_params.n_ctx);
    int n_tokens = llama_tokenize(
        vocab,
        prompt.c_str(),
        (int)prompt.length(),
        tokens_list.data(),
        tokens_list.size(),
        false,  // add_special (BOS/EOSを自動付与)
        false  // parse_special
    );
    if (n_tokens < 0) {
        std::cerr << "Tokenization failed." << std::endl;
        return "[ERROR: Tokenization failed]";
    }
    tokens_list.resize(n_tokens);

    // 初回デコード（プロンプト全体）
    llama_batch batch = llama_batch_init(n_tokens, 0, 1);
    for (int i = 0; i < n_tokens; ++i) {
        batch.token[i]    = tokens_list[i];
        batch.pos[i]      = i;
        batch.n_seq_id[i] = 1;
        batch.seq_id[i][0]= 0;
        batch.logits[i]   = false;
    }
    batch.logits[n_tokens - 1] = true;  // 最後のトークンの次を生成

    if (llama_decode(ctx, batch) != 0) {
        std::cerr << "llama_decode failed" << std::endl;
        llama_batch_free(batch);
        return "[ERROR: Decode failed]";
    }
    llama_batch_free(batch);

    // サンプラー初期化（グリーディサンプリング）
    llama_sampler *sampler = llama_sampler_init_greedy();

    std::string result;
    const int n_len = 128;
    int n_cur = n_tokens;

    for (int i = 0; i < n_len; ++i) {
        // サンプリングにより次トークン取得
        llama_token new_token_id = llama_sampler_sample(sampler, ctx, -1);
        llama_sampler_accept(sampler, new_token_id);

        // 終端判定（EOS/EOTなど）: llama_vocab_is_eog でチェック
        if (llama_vocab_is_eog(vocab, new_token_id)) {
            break;
        }

        // トークン→文字列変換（新API llama_token_to_piece）
        char buf[128] = {0};
        int len = llama_token_to_piece(vocab, new_token_id, buf, sizeof(buf), 0, true);
        if (len < 0) {
            std::cerr << "Failed to convert token to piece (id=" << new_token_id << ")" << std::endl;
            llama_sampler_free(sampler);
            return result;
        }
        result.append(buf, len);

        // 新トークンをデコードしてコンテキストを進める
        llama_batch batch_next = llama_batch_init(1, 0, 1);
        batch_next.token[0]    = new_token_id;
        batch_next.pos[0]      = n_cur;
        batch_next.n_seq_id[0] = 1;
        batch_next.seq_id[0][0]= 0;
        batch_next.logits[0]   = true;

        if (llama_decode(ctx, batch_next) != 0) {
            std::cerr << "Failed to decode next token" << std::endl;
            llama_batch_free(batch_next);
            break;
        }
        llama_batch_free(batch_next);
        n_cur++;
    }

    llama_sampler_free(sampler);
    return result;
}

void LLM::cleanup() {
    if (ctx) {
        llama_free(ctx);
        ctx = nullptr;
    }
    if (model) {
        llama_model_free(model);
        model = nullptr;
    }
}
