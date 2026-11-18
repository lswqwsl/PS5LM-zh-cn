// SPDX-License-Identifier: GPL-3.0-or-later
#include "model_plan.hpp"

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>

namespace ps5lm {

namespace {

constexpr double kGiB = 1073741824.0;

// GGUF metadata value types.
enum : uint32_t { U8, I8, U16, I16, U32, I32, F32, BOOL, STRING, ARRAY, U64, I64, F64 };

// A GGUF header from a file, or from memory (the start of a remote file);
// the PS5's libc has no fmemopen.
struct Reader {
    FILE *       f   = nullptr;
    const char * mem = nullptr;
    size_t       size = 0, pos = 0;
    bool         bad = false;

    size_t read(void * out, size_t n) {
        if (f) {
            return std::fread(out, 1, n, f);
        }
        const size_t k = pos + n <= size ? n : 0;
        std::memcpy(out, mem + pos, k);
        pos += k;
        return k;
    }
    template <typename T> T get() {
        T v{};
        if (read(&v, sizeof(v)) != sizeof(v)) {
            bad = true;
        }
        return v;
    }
    std::string str() {
        const uint64_t n = get<uint64_t>();
        if (bad || n > (1u << 24)) {
            bad = true;
            return {};
        }
        std::string s(n, '\0');
        if (n && read(s.data(), n) != n) {
            bad = true;
        }
        return s;
    }
    void skip(uint64_t n) {
        if (f ? std::fseek(f, (long) n, SEEK_CUR) != 0 : pos + n > size) {
            bad = true;
        }
        pos += f ? 0 : n;
    }
};

uint64_t scalar_size(uint32_t t) {
    switch (t) {
        case U8: case I8: case BOOL: return 1;
        case U16: case I16: return 2;
        case U32: case I32: case F32: return 4;
        case U64: case I64: case F64: return 8;
        default: return 0;
    }
}

// A scalar as an integer; 0 for strings and arrays.
uint64_t read_int(Reader & r, uint32_t t) {
    switch (t) {
        case U8: return r.get<uint8_t>();
        case I8: return (uint64_t) r.get<int8_t>();
        case U16: return r.get<uint16_t>();
        case I16: return (uint64_t) r.get<int16_t>();
        case U32: return r.get<uint32_t>();
        case I32: return (uint64_t) r.get<int32_t>();
        case U64: return r.get<uint64_t>();
        case I64: return (uint64_t) r.get<int64_t>();
        case BOOL: return r.get<uint8_t>();
        case F32: return (uint64_t) r.get<float>();
        case F64: return (uint64_t) r.get<double>();
        default: return 0;
    }
}

std::string lower(std::string s) {
    for (auto & c : s) {
        c = (char) std::tolower((unsigned char) c);
    }
    return s;
}

}  // namespace

// The vision projector for a model: an "mmproj" .gguf in the same folder.
// With several, the one whose name shares the longest start with the
// model's, once "mmproj" and separators are set aside
// ("mmproj-gemma-4-E4B-f16.gguf" for "gemma-4-E4B_q4_0-it.gguf").
std::string find_mmproj(const std::string & model_path) {
    const size_t slash = model_path.find_last_of('/');
    const std::string dir = model_path.substr(0, slash);
    const auto key = [](std::string n) {
        n = lower(n);
        if (const size_t at = n.find("mmproj"); at != std::string::npos) {
            n.erase(at, 6);
        }
        n.erase(std::remove_if(n.begin(), n.end(), [](char c) { return c == '-' || c == '_' || c == '.'; }), n.end());
        return n;
    };
    const std::string want = key(model_path.substr(slash + 1));
    std::string best;
    size_t      best_len = 0;
    DIR * d = opendir(dir.c_str());
    if (!d) {
        return best;
    }
    while (dirent * e = readdir(d)) {
        const std::string n = e->d_name;
        if (lower(n).find("mmproj") == std::string::npos || n.size() < 6 || lower(n.substr(n.size() - 5)) != ".gguf") {
            continue;
        }
        const std::string k = key(n);
        size_t common = 0;
        while (common < k.size() && common < want.size() && k[common] == want[common]) {
            ++common;
        }
        // At least the family's name in common ("gemma4e4b"), so a lone
        // projector of another model is not paired.
        if (common >= 5 && common > best_len) {
            best = dir + "/" + n;
            best_len = common;
        }
    }
    closedir(d);
    return best;
}

namespace {

// Reads a GGUF header from `f` (and closes it). `file_bytes` is the whole
// file's size when `f` holds only its start; 0 to measure `f`.
ModelInfo parse_model_info(Reader r, const std::string & path, uint64_t file_bytes) {
    FILE * f = r.f;
    ModelInfo m;
    m.path      = path;
    m.file_name = path.substr(path.find_last_of('/') + 1);
    m.name      = m.file_name;

    if (!f && !r.mem) {
        m.error = "打不开";
        return m;
    }
    if (r.get<uint32_t>() != 0x46554747u) {  // "GGUF"
        if (f) {
            std::fclose(f);
        }
        m.error = "不是 GGUF 文件";
        return m;
    }
    r.get<uint32_t>();  // version
    r.get<uint64_t>();  // tensor count
    const uint64_t n_kv = r.get<uint64_t>();

    std::map<std::string, uint64_t> ints;  // every scalar, and the largest element of numeric arrays
    std::map<std::string, uint64_t> sums;  // numeric arrays: the sum of their elements
    std::map<std::string, std::string> strs;
    for (uint64_t i = 0; i < n_kv && !r.bad; i++) {
        const std::string key = r.str();
        const uint32_t    t   = r.get<uint32_t>();
        if (t == STRING) {
            strs[key] = r.str();
        } else if (t == ARRAY) {
            const uint32_t et = r.get<uint32_t>();
            const uint64_t n  = r.get<uint64_t>();
            if (et == STRING) {
                for (uint64_t j = 0; j < n && !r.bad; j++) {
                    r.skip(r.get<uint64_t>());
                }
            } else if (n <= 4096) {  // per-layer values, such as head_count_kv
                uint64_t mx = 0, sum = 0;
                for (uint64_t j = 0; j < n; j++) {
                    const uint64_t v = read_int(r, et);
                    mx = std::max(mx, v);
                    sum += v;
                }
                ints[key] = mx;
                sums[key] = sum;
            } else {
                r.skip(n * scalar_size(et));  // vocabularies and the like
            }
        } else {
            ints[key] = read_int(r, t);
        }
    }
    if (f) {
        std::fseek(f, 0, SEEK_END);
    }
    m.file_bytes = file_bytes ? file_bytes : f ? (uint64_t) std::ftell(f) : r.size;
    if (f) {
        std::fclose(f);
    }
    if (r.bad) {
        m.error = "读不到元数据";
        return m;
    }

    m.arch = strs["general.architecture"];
    m.projector_type = strs["clip.projector_type"];
    if (!strs["general.name"].empty()) {
        m.name = strs["general.name"];
    }
    // PrismML's own packings of Bonsai models (their fork's types 142 and 143,
    // file types 141 and 143), which this llama.cpp refuses at load.
    if (ints.count("prism.hadamard.version") && ints.count("general.file_type")) {
        const uint64_t ftype = ints["general.file_type"];
        if (ftype == 141 || ftype == 143) {
            m.error = ftype == 141 ? "PrismML 的 PQ2_0：在 PC 上用 scripts/bonsai-pq2-to-q2.py 转成 Q2_0"
                                   : "PrismML 的 PTQ1_0，llama.cpp 加载不了；请下载它的 Q2_0 文件";
            return m;
        }
    }
    const std::string a = m.arch + ".";
    m.decision = strs[a + "decision.type"];
    auto num = [&](const std::string & k, uint64_t def = 0) {
        auto it = ints.find(a + k);
        return it == ints.end() ? def : it->second;
    };
    // block_count includes the MTP blocks some files carry (Qwen 3.8's
    // UD-Q2_K_XL has 65); they are not part of the main stack.
    m.n_layer         = (uint32_t) (num("block_count") - num("nextn_predict_layers"));
    m.n_ff            = (uint32_t) num("feed_forward_length");
    const uint64_t n_head = num("attention.head_count");
    m.n_head_kv       = (uint32_t) num("attention.head_count_kv", n_head);
    const uint64_t n_embd = num("embedding_length");
    const uint64_t head_dim = n_head ? n_embd / n_head : 0;
    m.key_length      = (uint32_t) num("attention.key_length", head_dim);
    m.value_length    = (uint32_t) num("attention.value_length", head_dim);
    m.full_attn_every = (uint32_t) std::max<uint64_t>(1, num("full_attention_interval", 1));
    m.n_ctx_train     = (uint32_t) num("context_length");
    // Sliding-window attention with its layers listed (true: a window layer),
    // as llama.cpp reads it. A pattern only in llama.cpp's code (Gemma) or a
    // single number is counted as full attention, which over-reserves.
    const auto pattern = sums.find(a + "attention.sliding_window_pattern");
    if (pattern != sums.end() && num("attention.sliding_window")) {
        m.n_swa_layers = (uint32_t) std::min<uint64_t>(pattern->second, m.n_kv_layers());
        m.swa_window   = (uint32_t) num("attention.sliding_window");
    }
    m.n_expert        = (uint32_t) num("expert_count");

    // Recurrent layers (Gated DeltaNet, Mamba): a fixed f32 state each,
    // the recurrent matrix plus the convolution window.
    const uint64_t inner = num("ssm.inner_size"), state = num("ssm.state_size");
    if (inner && state) {
        const uint64_t groups = num("ssm.group_count", 1), conv = num("ssm.conv_kernel", 4);
        const uint64_t n_rec  = m.n_layer - m.n_kv_layers();
        m.state_bytes = n_rec * (inner * state + (conv - 1) * (inner + 2 * groups * state)) * 4;
    }
    m.ok = m.n_layer > 0;
    if (!m.ok) {
        m.error = "架构 '" + m.arch + "' 没有给出层数";
    }
    return m;
}

}  // namespace

ModelInfo read_model_info(const std::string & path) {
    Reader r;
    r.f = std::fopen(path.c_str(), "rb");
    ModelInfo m = parse_model_info(r, path, 0);
    m.mmproj = find_mmproj(path);
    if (!m.mmproj.empty()) {
        struct stat st = {};
        m.mmproj_bytes = stat(m.mmproj.c_str(), &st) == 0 ? (uint64_t) st.st_size : 0;
        Reader pr;
        pr.f = std::fopen(m.mmproj.c_str(), "rb");
        m.projector_type = parse_model_info(pr, m.mmproj, 0).projector_type;
    }
    return m;
}

ModelInfo read_model_header(const std::string & name, const std::string & head, uint64_t file_bytes) {
    Reader r;
    r.mem  = head.data();
    r.size = head.size();
    return parse_model_info(r, name, file_bytes);
}

namespace {

double kv_bytes_per_layer_token(const ModelInfo & m, const std::string & type) {
    const double per_elem = type == "f16" ? 2.0 : type == "q8_0" ? 34.0 / 32 : 18.0 / 32;  // q4_0
    return (double) m.n_head_kv * (m.key_length + m.value_length) * per_elem;
}

}  // namespace

double kv_bytes_per_token(const ModelInfo & m, const std::string & type) {
    return (m.n_kv_layers() - m.n_swa_layers) * kv_bytes_per_layer_token(m, type);
}

// Sampling as each model's authors recommend it. Settings the planner works
// out (context, cache type) are not here.
const std::vector<Preset> & presets() {
    static const std::vector<Preset> table = {
        // PrismML's model card (thinking mode); min_p 0.05 is not in the file.
        { "bonsai", "Bonsai 2", "Qwen 3.8 27B 的三值权重；Q2_0 为 7.1 GiB，64k 上下文还有余量",
          { "--temp", "1.0", "--top-p", "0.95", "--top-k", "20", "--min-p", "0.05" }, 65536 },
        { "qwen3.8", "Qwen 3.8", "27B 混合 Gated DeltaNet；UD-Q2_K_XL 在 GPU 上能撑 64k 上下文",
          { "--temp", "0.6", "--top-p", "0.95", "--top-k", "20", "--min-p", "0" } },
        { "qwen3.6", "Qwen 3.6", "MoE 35B-A3B 或稠密 27B；MoE 解码最快",
          { "--temp", "0.6", "--top-p", "0.95", "--top-k", "20", "--min-p", "0" } },
        { "qwen3.5", "Qwen 3.5", "混合架构类似 3.8；9B 用 Q4_K_M 能留出长上下文空间",
          { "--temp", "0.6", "--top-p", "0.95", "--top-k", "20", "--min-p", "0" } },
        { "ornith", "Ornith 1.5", "基于 Qwen 3.5 的 9B",
          { "--temp", "0.6", "--top-p", "0.95", "--top-k", "20", "--min-p", "0" } },
        { "gemma", "Gemma 4", "12B 用 Q3_K_M / IQ4_XS，E4B 用 Q4_0",
          { "--temp", "1.0", "--top-p", "0.95", "--top-k", "64" } },
        { "gpt-oss", "gpt-oss", "20B MoE，激活 3.6B；只有 MXFP4 一种规格，上下文比较短",
          { "--temp", "1.0", "--top-p", "1.0" }, 16384 },
        { "granite", "Granite 4.2", "3B 与 8B，快而朴素",
          { "--temp", "0.0" } },
        { "nemotron", "Nemotron 3 Nano", "混合 Mamba，按 100 万上下文训练",
          { "--temp", "0.6", "--top-p", "0.95" } },
        { "mistral", "Mistral", "7B v0.3 和 Small 系列",
          { "--temp", "0.15" } },
        { "llama", "Llama 3", "8B 在任何 4bit 量化都放得下",
          { "--temp", "0.6", "--top-p", "0.9" } },
    };
    return table;
}

const Preset * find_preset(const ModelInfo & m) {
    if (!m.decision.empty()) {
        return nullptr;  // no sampling: a decision model reads its answers off one forward pass
    }
    const std::string hay = lower(m.file_name + " " + m.name + " " + m.arch);
    for (const auto & p : presets()) {
        if (hay.find(p.match) != std::string::npos) {
            return &p;
        }
    }
    return nullptr;
}

Plan plan_model(const ModelInfo & m, double budget_gib, uint32_t ctx_cap, const char * kv_force, uint32_t ubatch) {
    Plan p;
    const Preset * preset = find_preset(m);
    if (preset && preset->max_ctx) {
        ctx_cap = std::min(ctx_cap, preset->max_ctx);
    }
    // Decision prompts are short; 4k keeps the cache small for the wide batch below.
    if (!m.decision.empty()) {
        ctx_cap = std::min<uint32_t>(ctx_cap, 4096);
    }
    if (m.n_ctx_train) {
        ctx_cap = std::min(ctx_cap, m.n_ctx_train);
    }
    p.weights_gib = (m.file_bytes + m.mmproj_bytes) / kGiB;
    p.state_gib   = m.state_bytes / kGiB;
    // Compute buffers and the CPU side (the title heap shares the pool), plus
    // one prompt checkpoint of the recurrent state. Calibrated on the console:
    // Qwen3.8-27B UD-Q2_K_XL at 64k with a q4_0 cache ran with 11.38 GiB free.
    // ponytail: 0.25 GiB for the image encoder's buffers is a guess; measure on the console.
    // Decision models (laya, clef, vllm-d3, lfm2-d1-omni) and non-causal image
    // encoders (mtmd caps an image to the ubatch) need a whole prompt or image in
    // one ubatch, so they get 2048, the size #13 ran on the console.
    // A causal projector (Qwen's) takes the image in pieces: its model gets
    // kVisionUbatch and is not wide.
    const bool causal_vision = !m.mmproj.empty() && m.decision.empty() && causal_projector(m.projector_type);
    const bool wide = !m.decision.empty() || (!m.mmproj.empty() && !causal_vision);
    p.ubatch = wide ? 2048 : causal_vision ? kVisionUbatch : std::max<uint32_t>(ubatch, 1);
    // Compute buffers grow with the ubatch, by about 38 bytes a token for each
    // unit of feed-forward width: on Qwen 3.8 27B (17,408 wide), at 64k with a
    // q4_0 cache, ggml-vulkan's device and host compute buffers took 41 MiB
    // at -ub 64 and 321 MiB at 512, measured on a PC with the console's shader
    // paths. The 0.75 GiB above holds the 64 the console was calibrated at.
    const double ub_extra = !wide && p.ubatch > 64 ? (p.ubatch - 64) * (double) m.n_ff * 38.0 / kGiB : 0.0;
    // shortcut: 0.5 GiB more compute buffer for the 2048 ubatch is a guess; measure on the console.
    // A causal vision model keeps the 0.5 as headroom: each photo needs working
    // memory, and free memory shrank by about 0.5 GiB over 100 photos on the
    // console (Qwen 3.8 27B, -ub 256).
    p.reserve_gib = 0.75 + p.state_gib + (m.mmproj.empty() ? 0.0 : 0.25) + (wide || causal_vision ? 0.5 : 0.0) + ub_extra;

    // The longest context first, and at each length the best cache that fits.
    static const uint32_t ctxs[]  = { 262144, 196608, 131072, 65536, 32768, 16384, 8192, 4096 };
    static const char *   types[] = { "f16", "q8_0", "q4_0" };
    for (uint32_t ctx : ctxs) {
        if (ctx > ctx_cap) {
            continue;
        }
        for (const char * t : types) {
            if (kv_force && std::strcmp(t, kv_force) != 0) {
                continue;
            }
            // f16 only where it is cheap: q8_0 costs nothing measurable in quality.
            if (!kv_force && std::strcmp(t, "f16") == 0 && ctx > 8192) {
                continue;
            }
            // Window layers hold the window and one ubatch, padded to 256,
            // whatever the context (llama.cpp's iSWA cache without --swa-full).
            const uint32_t swa_tokens = std::min(ctx, (m.swa_window + p.ubatch + 255) / 256 * 256);
            const double   kv         = (kv_bytes_per_token(m, t) * ctx +
                                 m.n_swa_layers * kv_bytes_per_layer_token(m, t) * swa_tokens) / kGiB;
            const double total = p.weights_gib + kv + p.state_gib + p.reserve_gib;
            if (total <= budget_gib) {
                p.fits = true;
                p.ctx = ctx;
                p.kv_type = t;
                p.kv_gib = kv;
                p.total_gib = total;
                break;
            }
        }
        if (p.fits) {
            break;
        }
    }

    char why[160];
    if (!p.fits) {
        p.total_gib = p.weights_gib + p.state_gib + p.reserve_gib;
        std::snprintf(why, sizeof(why), "4k 上下文就要 %.1f GiB；可用 %.1f GiB", p.total_gib, budget_gib);
        p.why = why;
        return p;
    }
    std::snprintf(why, sizeof(why), "%s%uk 上下文，%s 缓存%s：%.1f / %.1f GiB",
                  m.decision.empty() ? "" : (std::string(kDecisionPlanTag) + m.decision + kDecisionPlanSep).c_str(),
                  p.ctx / 1024, p.kv_type.c_str(), m.mmproj.empty() ? "" : "，图像", p.total_gib, budget_gib);
    p.why = why;

    p.args = { "-m", m.path, "-ngl", "999", "-fit", "off", "-lm", "none",
               "-c", std::to_string(p.ctx), "-fa", "on", "-ctk", p.kv_type, "-ctv", p.kv_type,
               "-b", std::to_string(causal_vision ? kVisionImageCap : std::max<uint32_t>(256, p.ubatch)),
               "-ub", std::to_string(p.ubatch), "-np", "1",
               // One prompt checkpoint and no RAM cache: llama-server's defaults
               // (32 checkpoints, 8 GiB) are for a PC, and the CPU shares this pool.
               "-ctxcp", "1", "-cram", "0",
               "--host", "0.0.0.0", "--port", "8081" };
    if (!m.mmproj.empty()) {
        p.args.insert(p.args.end(), { "--mmproj", m.mmproj });
    }
    if (causal_vision) {
        p.args.insert(p.args.end(), { "--image-max-tokens", std::to_string(kVisionImageCap) });
    }
    if (preset) {
        p.args.insert(p.args.end(), preset->args.begin(), preset->args.end());
    }
    return p;
}

bool causal_projector(const std::string & projector_type) {
    return lower(projector_type).rfind("qwen", 0) == 0;
}

std::vector<std::string> find_models(const std::vector<std::string> & dirs) {
    std::vector<std::string> out;
    for (const auto & d : dirs) {
        DIR * dir = opendir(d.c_str());
        if (!dir) {
            continue;
        }
        while (dirent * e = readdir(dir)) {
            const std::string n = e->d_name;
            if (n.size() < 6 || lower(n.substr(n.size() - 5)) != ".gguf" || n[0] == '.') {
                continue;
            }
            // Split models: only the first part, which llama.cpp opens.
            const auto split = n.find("-of-");
            if (split != std::string::npos && n.find("-00001-of-") == std::string::npos) {
                continue;
            }
            // Vision projectors are not models on their own.
            if (lower(n).find("mmproj") != std::string::npos) {
                continue;
            }
            out.push_back(d + "/" + n);
        }
        closedir(dir);
    }
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace ps5lm
