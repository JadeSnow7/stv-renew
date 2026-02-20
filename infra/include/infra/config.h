#pragma once

#include <string>
#include <filesystem>
#include <cstdlib>

namespace stv::infra {

/// XDG Base Directory 规范配置路径管理
/// 参考：https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html
class XdgPaths {
public:
    /// 获取配置目录（~/.config/stv-renew 或 $XDG_CONFIG_HOME/stv-renew）
    static std::filesystem::path get_config_dir() {
        const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
        std::filesystem::path base;
        
        if (xdg_config && xdg_config[0] != '\0') {
            base = xdg_config;
        } else {
            const char* home = std::getenv("HOME");
            if (home && home[0] != '\0') {
                base = std::filesystem::path(home) / ".config";
            } else {
                base = "/tmp";  // 回退
            }
        }
        
        auto dir = base / "stv-renew";
        std::filesystem::create_directories(dir);
        return dir;
    }
    
    /// 获取缓存目录（~/.cache/stv-renew 或 $XDG_CACHE_HOME/stv-renew）
    static std::filesystem::path get_cache_dir() {
        const char* xdg_cache = std::getenv("XDG_CACHE_HOME");
        std::filesystem::path base;
        
        if (xdg_cache && xdg_cache[0] != '\0') {
            base = xdg_cache;
        } else {
            const char* home = std::getenv("HOME");
            if (home && home[0] != '\0') {
                base = std::filesystem::path(home) / ".cache";
            } else {
                base = "/tmp";
            }
        }
        
        auto dir = base / "stv-renew";
        std::filesystem::create_directories(dir);
        return dir;
    }
    
    /// 获取数据目录（~/.local/share/stv-renew 或 $XDG_DATA_HOME/stv-renew）
    static std::filesystem::path get_data_dir() {
        const char* xdg_data = std::getenv("XDG_DATA_HOME");
        std::filesystem::path base;
        
        if (xdg_data && xdg_data[0] != '\0') {
            base = xdg_data;
        } else {
            const char* home = std::getenv("HOME");
            if (home && home[0] != '\0') {
                base = std::filesystem::path(home) / ".local" / "share";
            } else {
                base = "/tmp";
            }
        }
        
        auto dir = base / "stv-renew";
        std::filesystem::create_directories(dir);
        return dir;
    }
    
    /// 获取输出目录（默认在 data_dir/outputs）
    static std::filesystem::path get_output_dir() {
        const char* custom_output = std::getenv("STV_OUTPUT_DIR");
        if (custom_output && custom_output[0] != '\0') {
            std::filesystem::path dir(custom_output);
            std::filesystem::create_directories(dir);
            return dir;
        }
        
        auto dir = get_data_dir() / "outputs";
        std::filesystem::create_directories(dir);
        return dir;
    }
};

/// 应用配置
struct AppConfig {
    std::string api_base_url = "http://127.0.0.1:8765";
    std::string output_dir;
    int max_retries = 2;
    int initial_backoff_ms = 500;
    int max_backoff_ms = 5000;
    
    static AppConfig from_environment() {
        AppConfig config;
        
        const char* api_base = std::getenv("STV_API_BASE_URL");
        if (api_base && api_base[0] != '\0') {
            config.api_base_url = api_base;
        }
        
        config.output_dir = XdgPaths::get_output_dir().string();
        
        const char* retries = std::getenv("STV_MAX_RETRIES");
        if (retries && retries[0] != '\0') {
            config.max_retries = std::stoi(retries);
        }
        
        return config;
    }
};

} // namespace stv::infra
