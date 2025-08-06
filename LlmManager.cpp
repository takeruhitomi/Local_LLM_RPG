// LlmManager.cpp (スレッド数最適化版)

#include "LlmManager.h"
#include <stdexcept>
#include <iostream>
#include <vector>
#include <sstream>
#include <algorithm>
#include <set>

LlmManager::LlmManager(const std::map<std::string, std::string>& model_paths) {
    llama_backend_init();

    // モデルファイルパスでグループ化してインスタンスを共有
    std::map<std::string, LlmInstance*> shared_instances_by_path;
    
    for (const auto& pair : model_paths) {
        const std::string& role = pair.first;
        const std::string& path = pair.second;

        // 同じパスのモデルが既に読み込まれているかチェック
        auto path_it = shared_instances_by_path.find(path);
        if (path_it != shared_instances_by_path.end()) {
            // 既存のインスタンスを共有
            instances[role] = *(path_it->second);
            std::cout << "Role '" << role << "' shares model instance from: " << path << std::endl;
            continue;
        }

        // 新しいモデルインスタンスを作成
        LlmInstance instance;
        auto mparams = llama_model_default_params();
        mparams.use_mmap = false;  // メモリマップを無効化してCPU使用率向上
        mparams.use_mlock = false;
        
        instance.model = llama_model_load_from_file(path.c_str(), mparams);
        if (instance.model == nullptr) {
            throw std::runtime_error("Error: failed to load model for role '" + role + "' from " + path);
        }

        auto cparams = llama_context_default_params();
        cparams.n_ctx = 2048;  // コンテキストサイズを維持
        cparams.n_batch = 256; // バッチサイズを削減してエラー回避
        
        // スレッド数の最適化
        cparams.n_threads = 8;        // 物理コア数に近い値
        cparams.n_threads_batch = 8;  // バッチ処理用も同じ
        
        // CPU最適化設定
        cparams.flash_attn = false;
        cparams.offload_kqv = false;  // KQVキャッシュをCPUに保持
        
        instance.ctx = llama_init_from_model(instance.model, cparams);
        if (instance.ctx == nullptr) {
            llama_model_free(instance.model);
            throw std::runtime_error("Error: failed to create context for role '" + role + "'");
        }
        
        instances[role] = instance;
        shared_instances_by_path[path] = &instances[role];
        std::cout << "New model instance for role '" << role << "' loaded from: " << path << std::endl;
    }
}

LlmManager::~LlmManager() {
    // 共有インスタンスの重複解放を防ぐために、ユニークポインタで管理
    std::set<llama_context*> freed_contexts;
    std::set<llama_model*> freed_models;
    
    for (auto& pair : instances) {
        if (pair.second.ctx && freed_contexts.find(pair.second.ctx) == freed_contexts.end()) {
            llama_free(pair.second.ctx);
            freed_contexts.insert(pair.second.ctx);
        }
        if (pair.second.model && freed_models.find(pair.second.model) == freed_models.end()) {
            llama_model_free(pair.second.model);
            freed_models.insert(pair.second.model);
        }
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

    // プロンプト表示は主要なもののみ
    if (role == "GM") {
        std::cout << "\n=== GM PROMPT SENT ===\n";
        std::cout << prompt << std::endl;
        std::cout << "=== END GM PROMPT ===\n" << std::endl;
    }

    llama_memory_clear(llama_get_memory(instance.ctx), false);
    
    // バッチサイズをより安全な値に設定
    int safe_batch_size = std::min(n_tokens, 256);  // 512から256に削減
    llama_batch batch = llama_batch_init(safe_batch_size, 0, 1);
    
    // バッチ処理を複数回に分割して実行
    int processed_tokens = 0;
    while (processed_tokens < n_tokens) {
        int current_batch_size = std::min(safe_batch_size, n_tokens - processed_tokens);
        
        // バッチの各トークンを設定
        for (int32_t i = 0; i < current_batch_size; ++i) {
            batch.token[i] = tokens_list[processed_tokens + i];
            batch.pos[i] = processed_tokens + i;
            batch.n_seq_id[i] = 1;
            batch.seq_id[i][0] = 0;
            batch.logits[i] = false;
        }
        batch.n_tokens = current_batch_size;
        
        // 最後のトークンでのみlogitsを有効化（生成に必要）
        if (processed_tokens + current_batch_size >= n_tokens) {
            batch.logits[current_batch_size - 1] = true;
        }

        if (llama_decode(instance.ctx, batch) != 0) {
            llama_batch_free(batch);
            return "[ERROR: llama_decode failed]";
        }
        
        processed_tokens += current_batch_size;
    }
    llama_batch_free(batch);

    std::string result_str;
    int n_cur = n_tokens;
    
    // ★★★ Llama3用のサンプラー設定 ★★★
    struct llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
    llama_sampler* sampler = llama_sampler_chain_init(sparams);
    
    if (role == "GM") {
        // JSON生成用：より確定的
        llama_sampler_chain_add(sampler, llama_sampler_init_temp(0.3f));
        llama_sampler_chain_add(sampler, llama_sampler_init_top_k(20));
        llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.85f, 1));
    } else {
        // NPC会話用：自然な多様性
        llama_sampler_chain_add(sampler, llama_sampler_init_temp(0.7f));
        llama_sampler_chain_add(sampler, llama_sampler_init_top_k(35));
        llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.9f, 1));
    }
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(1234));
    
    // ★★★ 生成用バッチは常に1トークンのみ ★★★
    llama_batch gen_batch = llama_batch_init(1, 0, 1);

    bool has_started_json = false;
    int brace_count = 0;
    
    std::vector<std::string> stop_tokens = {"<|eot_id|>", "<|end_of_text|>", "[/GPT]", "</s>"};
    
    // トークン数制限を更に削減してエラーを回避
    int max_tokens = (role == "GM" || role == "BATTLE") ? 150 : 80;

    // リアルタイム表示は重要な情報のみ
    if (role == "GM") {
        std::cout << "=== GM RAW OUTPUT ===\n";
    }

    for (int i = 0; i < max_tokens; ++i) {
        llama_token new_token_id = llama_sampler_sample(sampler, instance.ctx, -1);
        llama_sampler_accept(sampler, new_token_id);

        if (llama_vocab_is_eog(vocab, new_token_id)) break;

        char piece[128] = {0};
        int len = llama_token_to_piece(vocab, new_token_id, piece, sizeof(piece), 0, false);
        if (len < 0) break;
        std::string piece_str(piece, len);

        result_str.append(piece_str);
        
        // GM出力のみリアルタイム表示
        if (role == "GM") {
            std::cout << piece_str << std::flush;
        }

        // 停止トークンのチェック
        for (const auto& stop_token : stop_tokens) {
            if (result_str.find(stop_token) != std::string::npos) {
                goto generation_done;
            }
        }

        // 役割別の終了条件判定
        if (role == "GM") {
            // GM用：JSON形式の完了を検出
            if (!has_started_json && piece_str.find('{') != std::string::npos) {
                has_started_json = true;
            }
            if (has_started_json) {
                for(char c : piece_str) {
                    if (c == '{') brace_count++;
                    if (c == '}') brace_count--;
                }
                if (brace_count <= 0 && has_started_json) break;
            }
        } else if (role == "BATTLE") {
            // 戦闘用：JSON形式の完了を検出
            if (!has_started_json && piece_str.find('{') != std::string::npos) {
                has_started_json = true;
            }
            if (has_started_json) {
                for(char c : piece_str) {
                    if (c == '{') brace_count++;
                    if (c == '}') brace_count--;
                }
                if (brace_count <= 0 && has_started_json) break;
            }
        } else {
            // NPC会話の自然な終了
            if ((piece_str.find("。") != std::string::npos || 
                 piece_str.find("！") != std::string::npos || 
                 piece_str.find("？") != std::string::npos) && 
                result_str.length() > 20) {
                break;
            }
        }
        
        // ★★★ 生成バッチの設定を安全に行う ★★★
        gen_batch.n_tokens = 1;
        gen_batch.token[0] = new_token_id;
        gen_batch.pos[0] = n_cur;
        gen_batch.n_seq_id[0] = 1;
        gen_batch.seq_id[0][0] = 0;
        gen_batch.logits[0] = true;
        
        // バッチサイズチェック（コンテキストサイズ制限）
        if (n_cur >= 2048 - 1) {  // コンテキストサイズの上限近くで停止
            std::cout << "\n[WARNING: Context size limit reached, stopping generation]" << std::endl;
            break;
        }
        
        if (llama_decode(instance.ctx, gen_batch) != 0) {
            std::cout << "\n[WARNING: llama_decode failed, stopping generation]" << std::endl;
            break;
        }
        n_cur++;
    }
    
generation_done:
    llama_batch_free(gen_batch);
    llama_sampler_free(sampler);
    
    // ★★★ 戦闘用の出力表示を削除 ★★★
    // if (role == "BATTLE") {
    //     std::cout << std::endl;
    //     std::cout << "=== BATTLE RAW OUTPUT ===\n";
    //     std::cout << "\"" << result_str << "\"" << std::endl;
    //     std::cout << "=== END BATTLE RAW OUTPUT ===\n" << std::endl;
    // }

    // ★★★ 戦闘用のクリーンアップ後表示も削除 ★★★
    // if (role == "BATTLE") {
    //     std::cout << "=== BATTLE AFTER CLEANUP ===\n";
    //     std::cout << "\"" << result_str << "\"" << std::endl;
    //     std::cout << "=== END BATTLE AFTER CLEANUP ===\n" << std::endl;
    // }
    
    // ★★★ GM情報のみ詳細表示 ★★★
    if (role == "GM") {
        std::cout << std::endl;
        std::cout << "=== END GM RAW OUTPUT ===\n";
        std::cout << "=== GM BEFORE CLEANUP ===\n";
        std::cout << "\"" << result_str << "\"" << std::endl;
        std::cout << "=== END GM BEFORE CLEANUP ===\n" << std::endl;
    }
    
    // ★★★ 最終的なクリーンアップ（戦闘用はより慎重に） ★★★
    if (role == "BATTLE") {
        // 戦闘用は最小限のクリーンアップのみ
        std::vector<std::string> battle_cleanup_tokens = {
            "<|eot_id|>", "<|end_of_text|>", "</s>"
        };
        
        for (const auto& token : battle_cleanup_tokens) {
            size_t pos = result_str.find(token);
            if (pos != std::string::npos) {
                result_str = result_str.substr(0, pos);
            }
        }
    } else {
        // 通常のクリーンアップ
        std::vector<std::string> cleanup_tokens = {
            "<|eot_id|>", "<|start_header_id|>", "<|end_header_id|>", 
            "<|begin_of_text|>", "[/GPT]", "[GPT]", "</s>", "<s>", "<|end_of_text|>"
        };
        
        for (const auto& token : cleanup_tokens) {
            size_t pos = result_str.find(token);
            if (pos != std::string::npos) {
                result_str = result_str.substr(0, pos);
            }
        }
    }
    
    // ★★★ 戦闘結果のクリーンアップ後も表示 ★★★
    if (role == "BATTLE") {
        std::cout << "=== BATTLE AFTER CLEANUP ===\n";
        std::cout << "\"" << result_str << "\"" << std::endl;
        std::cout << "=== END BATTLE AFTER CLEANUP ===\n" << std::endl;
    }
    
    // ★★★ GM情報のみ後処理結果表示 ★★★
    if (role == "GM") {
        std::cout << "=== GM AFTER CLEANUP ===\n";
        std::cout << "\"" << result_str << "\"" << std::endl;
        std::cout << "=== END GM AFTER CLEANUP ===\n" << std::endl;
    }
    
    return result_str;
}

std::string LlmManager::generateNpcDialogue(const std::vector<ChatMessage>& history, const std::string& scene_context) {
    // ★★★ ゲーム内ストーリー設定に合わせて修正 ★★★
    std::string world_lore = 
        "=== 世界設定 ===\n"
        "かつて、世界は万物の調和を司る「調和のクリスタル」の恩恵を受け、平和と繁栄を謳歌していた。\n"
        "しかし、ある日、どこからともなく現れた謎の災厄「静寂」が世界を覆い始める。\n"
        "「静寂」は生命力と色彩を奪い、世界を無音の灰色に変えていく恐ろしい現象である。\n\n"
        "物語は、世界の片隅にある「始まりの村」から始まる。\n"
        "この村は古い結界によって「静寂」から守られているが、結界の力は年々弱くなっている。\n"
        "村のすぐそばには「静寂の森」が広がり、そこには「静寂」に侵された魔物たちが徘徊している。\n\n"
        "村の長老（あなた）は古代の知識を持つ賢者で、「調和のクリスタル」を復活させる方法を探している。\n"
        "最近の研究で、森の奥深くにある古い神殿に手がかりがあると分かったが、\n"
        "そこに辿り着くには多くの危険を乗り越えなければならない。\n"
        "長老は新たな勇者の到来を待ち望んでいる。\n\n";

    std::string system_prompt =
        world_lore +
        "=== あなたの役割 ===\n"
        "あなたは始まりの村の長老です。プレイヤーに対して親切で知恵深い助言をしてください。\n"
        "現在の状況: " + scene_context + "\n\n"
        "特徴：\n"
        "- 長老らしい落ち着いた口調で話す（「〜じゃ」「〜のう」などの語尾を使用）\n"
        "- プレイヤーの発言に適切に反応する\n"
        "- 「静寂」の脅威と「調和のクリスタル」について詳しく知っている\n"
        "- 若者を励まし、希望を与えようとする\n"
        "- 古代の知識や魔法について語ることができる\n"
        "- 村の結界や森の危険について警告する\n\n"
        "親しみやすい日本語で、長老のセリフのみを出力してください。";

    std::stringstream ss;
    ss << "<|begin_of_text|><|start_header_id|>system<|end_header_id|>\n\n" << system_prompt << "<|eot_id|>";
    
    // ★★★ 最初の挨拶を追加 ★★★
    bool has_initial_greeting = false;
    for (const auto& msg : history) {
        if (msg.role == "assistant" && !msg.content.empty()) {
            has_initial_greeting = true;
            break;
        }
    }
    
    // 最初の挨拶がない場合は長老の初期セリフを追加
    if (!has_initial_greeting && history.size() <= 1) {
        ss << "<|start_header_id|>assistant<|end_header_id|>\n\n長老: あなたか...。よく来てくれた。話したいことがある。<|eot_id|>";
    }
    
    // ★★★ 履歴を増やして文脈理解向上 ★★★
    int start_idx = std::max(0, (int)history.size() - 5);
    for (int i = start_idx; i < history.size(); ++i) {
        const auto& msg = history[i];
        if (msg.role == "system") continue;
        if (msg.role == "user") {
            ss << "<|start_header_id|>user<|end_header_id|>\n\nプレイヤー: " << msg.content << "<|eot_id|>";
        } else if (msg.role == "assistant") {
            ss << "<|start_header_id|>assistant<|end_header_id|>\n\n長老: " << msg.content << "<|eot_id|>";
        }
    }
    ss << "<|start_header_id|>assistant<|end_header_id|>\n\n長老: ";
    
    std::string raw_response = run_inference("NPC", ss.str());
    
    // ★★★ デバッグ用：NPC応答の前処理（プロンプトのみ表示） ★★★
    std::cout << "=== NPC PROMPT SENT ===\n";
    std::cout << ss.str() << std::endl;
    std::cout << "=== END NPC PROMPT ===\n" << std::endl;
    
    // ★★★ レスポンスのクリーンアップを強化 ★★★
    // Llama3の特殊トークンを除去
    std::vector<std::string> tokens_to_remove = {
        "<|eot_id|>", "<|start_header_id|>", "<|end_header_id|>", 
        "<|begin_of_text|>", "[/GPT]", "[GPT]", "</s>", "<s>", "長老: "
    };
    
    for (const auto& token : tokens_to_remove) {
        size_t pos = raw_response.find(token);
        if (pos != std::string::npos) {
            raw_response = raw_response.substr(0, pos);
        }
    }
    
    // 不要な文字を削除
    size_t first = raw_response.find_first_not_of(" \n\r\t");
    if (first != std::string::npos) {
        size_t last = raw_response.find_last_not_of(" \n\r\t");
        raw_response = raw_response.substr(first, last - first + 1);
    }
    
    // ★★★ デバッグ用：NPC応答の後処理結果を表示 ★★★
    std::cout << "=== NPC RAW RESPONSE ===\n";
    std::cout << "\"" << raw_response << "\"" << std::endl;
    std::cout << "=== END NPC RAW RESPONSE ===\n" << std::endl;
    
    return raw_response;
}

// ★★★ GMのプロンプトも世界設定に合わせて微調整 ★★★
GmResponse LlmManager::generateGmResponse(const std::vector<ChatMessage>& history) {
    std::string system_prompt =
        "あなたは日本語RPGのゲームマスターです。プレイヤーとの会話を分析し、JSON形式で応答してください。\n\n"
        "世界設定：\n"
        "- 世界は「静寂」という災厄に脅かされている\n"
        "- プレイヤーは「始まりの村」にいる\n"
        "- 長老は「調和のクリスタル」を復活させる方法を探している\n"
        "- 森には「静寂」に侵された魔物がいる\n\n"
        "必須フォーマット：\n"
        "{\n"
        "  \"scene_context\": \"現在の状況や雰囲気の説明\",\n"
        "  \"action\": \"CONTINUE/DEPART\",\n"
        "  \"items\": [\"アイテム名1\", \"アイテム名2\"]\n"
        "}\n\n"
        "判断基準：\n"
        "- プレイヤーが冒険に出発する意思を明確に示した場合：action=\"DEPART\"\n"
        "- その他の場合：action=\"CONTINUE\"\n"
        "- DEPART時は items に [\"初心者の剣\", \"革の鎧\"] を設定\n\n"
        "プレイヤーの発言内容と文脈を十分に考慮して判断してください。";

    std::stringstream ss;
    ss << "<|begin_of_text|><|start_header_id|>system<|end_header_id|>\n\n" << system_prompt << "<|eot_id|>";
    
    // ★★★ 履歴を増やして文脈理解向上 ★★★
    int start_idx = std::max(0, (int)history.size() - 6);
    for (int i = start_idx; i < history.size(); ++i) {
        const auto& msg = history[i];
        if (msg.role == "system") continue;
        if (msg.role == "user") {
            ss << "<|start_header_id|>user<|end_header_id|>\n\nプレイヤー: " << msg.content << "<|eot_id|>";
        } else if (msg.role == "assistant") {
            ss << "<|start_header_id|>assistant<|end_header_id|>\n\n長老: " << msg.content << "<|eot_id|>";
        }
    }
    ss << "<|start_header_id|>user<|end_header_id|>\n\n上記の会話を分析してください。<|eot_id|>";
    ss << "<|start_header_id|>assistant<|end_header_id|>\n\n";

    std::string raw_response = run_inference("GM", ss.str());
    
    // ★★★ デバッグ用：パース前の応答を表示 ★★★
    std::cout << "=== GM RESPONSE BEFORE PARSING ===\n";
    std::cout << "\"" << raw_response << "\"" << std::endl;
    std::cout << "=== END GM RESPONSE ===\n" << std::endl;
    
    GmResponse result = parseGmResponse(raw_response);
    
    // ★★★ デバッグ用：パース結果を表示 ★★★
    std::cout << "=== PARSED GM RESPONSE ===\n";
    std::cout << "scene_context: \"" << result.scene_context << "\"" << std::endl;
    std::cout << "action: \"" << result.action << "\"" << std::endl;
    std::cout << "items: [";
    for (size_t i = 0; i < result.items.size(); ++i) {
        std::cout << "\"" << result.items[i] << "\"";
        if (i < result.items.size() - 1) std::cout << ", ";
    }
    std::cout << "]" << std::endl;
    std::cout << "=== END PARSED RESPONSE ===\n" << std::endl;
    
    return result;
}

// ★★★ 戦闘システムも世界設定に合わせて調整 ★★★
BattleResponse LlmManager::generateBattleResponse(const std::string& player_stats, const std::string& enemy_stats, const std::string& player_action, const std::string& enemy_info) {
    std::string system_prompt =
        "あなたは戦闘の裁定者です。「静寂」に侵された魔物との戦いを裁定します。\n"
        "以下のステータスを持つキャラクターが、指定された方法で攻撃します。\n"
        "この攻撃がどの程度のダメージを与えるか、命中するか、そして何か追加効果が発生するかを判断し、\n"
        "以下のJSON形式で結果を返してください。JSON以外のテキストは絶対に出力してはいけません。\n\n"
        "{\n"
        "  \"damage\": 数値,\n"
        "  \"hit\": true/false,\n"
        "  \"effect_text\": \"追加効果の説明テキスト\"\n"
        "}\n\n"
        "攻撃側ステータス: " + player_stats + "\n"
        "防御側ステータス: " + enemy_stats + "\n"
        "敵の情報: " + enemy_info + "\n"
        "攻撃方法: " + player_action + "\n\n"
        "判定基準：\n"
        "- 基本ダメージは攻撃力から防御力を引いた値\n"
        "- 攻撃方法が敵の弱点に該当する場合、ダメージを1.5～2倍に増加\n"
        "- 命中率は攻撃方法の妥当性で判断（通常80-90%）\n"
        "- 弱点攻撃の場合はeffect_textで弱点を突いたことを説明";

    std::stringstream ss;
    ss << "<|begin_of_text|><|start_header_id|>system<|end_header_id|>\n\n" << system_prompt << "<|eot_id|>";
    ss << "<|start_header_id|>assistant<|end_header_id|>\n\n";
    
    std::string raw_response = run_inference("BATTLE", ss.str());
    
    BattleResponse result = parseBattleResponse(raw_response);
    
    // ★★★ 簡潔な結果表示のみ ★★★
    std::cout << "=== BATTLE RESULT ===\n";
    std::cout << "damage: " << result.damage << std::endl;
    std::cout << "hit: " << (result.hit) << std::endl;
    std::cout << "effect_text: " << result.effect_text << std::endl;
    std::cout << "==================\n" << std::endl;
    
    return result;
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

BattleResponse LlmManager::parseBattleResponse(const std::string& raw_str) {
    BattleResponse res;
    
    size_t start_pos = raw_str.find('{');
    size_t end_pos = raw_str.rfind('}');
    
    if (start_pos == std::string::npos || end_pos == std::string::npos || start_pos >= end_pos) {
        res.damage = 0;
        res.hit = false;
        res.effect_text = "（攻撃ははずれた...）";
        return res;
    }
    
    std::string json_str = raw_str.substr(start_pos, end_pos - start_pos + 1);

    auto getValue = [&](const std::string& key) -> std::string {
        std::string search_key = "\"" + key + "\"";
        size_t key_pos = json_str.find(search_key);
        if (key_pos == std::string::npos) return "";
        
        size_t colon_pos = json_str.find(":", key_pos);
        if (colon_pos == std::string::npos) return "";
        
        size_t value_start = colon_pos + 1;
        while (value_start < json_str.length() && 
               (json_str[value_start] == ' ' || json_str[value_start] == '\t' || 
                json_str[value_start] == '\n' || json_str[value_start] == '\r')) {
            value_start++;
        }
        
        if (value_start >= json_str.length()) return "";
        
        char start_char = json_str[value_start];
        
        if (start_char == '"') {
            size_t value_end = value_start + 1;
            while (value_end < json_str.length()) {
                if (json_str[value_end] == '"' && json_str[value_end - 1] != '\\') {
                    break;
                }
                value_end++;
            }
            
            if (value_end < json_str.length()) {
                return json_str.substr(value_start + 1, value_end - value_start - 1);
            }
        } else if (std::isdigit(start_char) || start_char == 't' || start_char == 'f') {
            size_t value_end = value_start;
            while (value_end < json_str.length() && 
                   json_str[value_end] != ',' && json_str[value_end] != '}' && 
                   json_str[value_end] != '\n' && json_str[value_end] != '\r') {
                value_end++;
            }
            
            std::string result = json_str.substr(value_start, value_end - value_start);
            while (!result.empty() && (result.back() == ' ' || result.back() == '\t')) {
                result.pop_back();
            }
            return result;
        }
        
        return "";
    };

    res.damage = 0;
    res.hit = false;
    res.effect_text = "攻撃は外れた...";

    try {
        std::string damage_str = getValue("damage");
        if (!damage_str.empty()) {
            res.damage = std::stoi(damage_str);
        }
    } catch (...) { 
        res.damage = 0; 
    }

    std::string hit_str = getValue("hit");
    res.hit = (hit_str == "true");
    
    std::string effect_text = getValue("effect_text");
    if (!effect_text.empty()) {
        res.effect_text = effect_text;
    } else if (res.hit) {
        res.effect_text = "攻撃が命中した！";
    } else {
        res.effect_text = "攻撃は外れた...";
    }

    return res;
}
