#ifndef __TextureManager__
#define __TextureManager__

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
// #include <imgui/backends/imgui_impl_sdl2.h>
// #include <imgui/backends/imgui_impl_sdlrenderer2.h>  // Updated backend header
// #include <imgui/imgui.h>
#include <nanobind/nanobind.h>

#include <functional>
#include <map>
#include <string>
#include <unordered_map>

// #include "World.hpp"

namespace nb = nanobind;

// Function to load an image and create a texture
uintptr_t load_texture(uintptr_t renderer_ptr, const std::string& image_path);

// Function to render the texture
void render_texture(uintptr_t renderer_ptr, uintptr_t texture_ptr, int x, int y);

// Function to destroy the texture
void destroy_texture(uintptr_t texture_ptr);

// typedef TextureManager TheTextureManager;

void loadTextureOnManager(uintptr_t renderer_ptr, const std::string& imagePath,
                          const std::string& id, int newWidth, int newHeight);

void renderTextureFromManager(uintptr_t renderer_ptr, const std::string& id, int x, int y);

SDL_Texture* getTextureFromManager(const std::string& id);

class TextureManager {
   public:
    // Meyer's Singleton pattern - thread-safe in C++11 and later
    static TextureManager* Instance() {
        static TextureManager instance;  // Guaranteed to be initialized only once
        return &instance;
    }

    bool load(std::string fileName, std::string id, SDL_Renderer* pRenderer, int newWidth,
              int newHeight);
    void draw(std::string id, int x, int y, int width, int height, SDL_Renderer* pRenderer,
              SDL_RendererFlip flip = SDL_FLIP_NONE);
    SDL_Texture* getTexture(std::string id);

    // void drawFrame(std::string id, int x, int y, int width, int height, int currentRow, int
    // currentFrame, SDL_Renderer *pRenderer, SDL_RendererFlip flip = SDL_FLIP_NONE);

    std::map<std::string, SDL_Texture*> m_textureMap;

   private:
    // Private constructor and destructor for singleton pattern
    TextureManager() {}
    ~TextureManager() {}

    // Delete copy constructor and assignment operator
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;
};

typedef TextureManager TheTextureManager;

#endif /* defined(__SDL_Game_Programming_Book__TextureManager__) */