#ifndef FONTMANAGER_HPP
#define FONTMANAGER_HPP

#include <SDL2/SDL_ttf.h>

#include <map>
#include <memory>
#include <string>

class FontManager {
   public:
    static FontManager* Instance() {
        static FontManager instance;
        return &instance;
    }

    // Load a font with a specific size
    bool loadFont(const std::string& font_id, const std::string& file_path, int font_size) {
        TTF_Font* font = TTF_OpenFont(file_path.c_str(), font_size);
        if (!font) {
            SDL_Log("TTF_OpenFont Error: %s", TTF_GetError());
            return false;
        }
        fonts_[font_id] = font;
        return true;
    }

    // Get a font by ID
    TTF_Font* getFont(const std::string& font_id) const {
        auto it = fonts_.find(font_id);
        if (it != fonts_.end()) {
            return it->second;
        }
        return nullptr;
    }

    // Clean up all loaded fonts
    void clean() {
        for (auto& pair : fonts_) {
            TTF_CloseFont(pair.second);
        }
        fonts_.clear();
    }

   private:
    FontManager() = default;
    ~FontManager() { clean(); }
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;

    std::map<std::string, TTF_Font*> fonts_;
};

#endif  // FONTMANAGER_HPP