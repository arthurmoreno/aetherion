#include "TextureManager.hpp"

#include <algorithm>
#include <iostream>

uintptr_t load_texture(uintptr_t renderer_ptr, const std::string& image_path) {
    SDL_Renderer* renderer = reinterpret_cast<SDL_Renderer*>(renderer_ptr);

    // Load image surface
    SDL_Surface* surface = IMG_Load(image_path.c_str());
    if (!surface) {
        throw std::runtime_error(std::string("Failed to load image: ") + IMG_GetError());
    }

    // Create texture from surface
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    if (!texture) {
        throw std::runtime_error(std::string("Failed to create texture: ") + SDL_GetError());
    }

    // Return the texture pointer as an integer
    return reinterpret_cast<uintptr_t>(texture);
}

// Function to render the texture
void render_texture(uintptr_t renderer_ptr, uintptr_t texture_ptr, int x, int y) {
    SDL_Renderer* renderer = reinterpret_cast<SDL_Renderer*>(renderer_ptr);
    SDL_Texture* texture = reinterpret_cast<SDL_Texture*>(texture_ptr);

    // Get texture width and height
    int w, h;
    if (SDL_QueryTexture(texture, nullptr, nullptr, &w, &h) != 0) {
        throw std::runtime_error(std::string("SDL_QueryTexture failed: ") + SDL_GetError());
    }

    // Set destination rectangle
    SDL_Rect dst_rect = {x, y, w, h};

    // Render the texture
    if (SDL_RenderCopy(renderer, texture, nullptr, &dst_rect) != 0) {
        throw std::runtime_error(std::string("SDL_RenderCopy failed: ") + SDL_GetError());
    }
}

// Function to destroy the texture
void destroy_texture(uintptr_t texture_ptr) {
    SDL_Texture* texture = reinterpret_cast<SDL_Texture*>(texture_ptr);
    SDL_DestroyTexture(texture);
}

void loadTextureOnManager(uintptr_t renderer_ptr, const std::string& imagePath,
                          const std::string& id, int newWidth, int newHeight) {
    SDL_Renderer* renderer = reinterpret_cast<SDL_Renderer*>(renderer_ptr);

    TextureManager::Instance()->load(imagePath, id, renderer, newWidth, newHeight);
}

void renderTextureFromManager(uintptr_t renderer_ptr, const std::string& id, int x, int y) {
    SDL_Renderer* renderer = reinterpret_cast<SDL_Renderer*>(renderer_ptr);

    // Get texture width and height
    int w, h;
    if (SDL_QueryTexture(TextureManager::Instance()->m_textureMap[id], nullptr, nullptr, &w, &h) !=
        0) {
        throw std::runtime_error(std::string("SDL_QueryTexture failed: ") + SDL_GetError());
    }

    // Set destination rectangle
    SDL_Rect dst_rect = {x, y, w, h};

    // Render the texture
    if (SDL_RenderCopy(renderer, TextureManager::Instance()->m_textureMap[id], nullptr,
                       &dst_rect) != 0) {
        throw std::runtime_error(std::string("SDL_RenderCopy failed: ") + SDL_GetError());
    }
}

SDL_Texture* getTextureFromManager(const std::string& id) {
    auto it = TextureManager::Instance()->m_textureMap.find(id);
    if (it != TextureManager::Instance()->m_textureMap.end()) {
        SDL_Texture* texture = it->second;
        if (!texture) {
            std::cerr << "Texture with ID '" << id << "' is null." << std::endl;
        }
        return texture;
    } else {
        std::cerr << "Texture with ID '" << id << "' not found in TextureManager." << std::endl;
        return nullptr;
    }
}

bool TextureManager::load(std::string fileName, std::string id, SDL_Renderer* pRenderer,
                          int newWidth, int newHeight) {
    // Load the image as a surface
    SDL_Surface* pTempSurface = IMG_Load(fileName.c_str());

    if (pTempSurface == nullptr) {
        throw std::runtime_error("IMG_Load failed: " + std::string(IMG_GetError()));
    }

    // Create a texture from the surface
    SDL_Texture* pTexture = SDL_CreateTextureFromSurface(pRenderer, pTempSurface);
    SDL_FreeSurface(pTempSurface);  // Free the surface as it's no longer needed

    if (pTexture != nullptr) {
        if (newWidth > 0 && newHeight > 0) {
            // Create a scaled texture with target access
            SDL_Texture* pScaledTexture = SDL_CreateTexture(
                pRenderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, newWidth, newHeight);

            if (pScaledTexture == nullptr) {
                SDL_DestroyTexture(pTexture);
                throw std::runtime_error("SDL_CreateTexture failed: " +
                                         std::string(SDL_GetError()));
            }

            // Set the blend mode for the scaled texture to handle transparency
            if (SDL_SetTextureBlendMode(pScaledTexture, SDL_BLENDMODE_BLEND) != 0) {
                SDL_DestroyTexture(pTexture);
                SDL_DestroyTexture(pScaledTexture);
                throw std::runtime_error("SDL_SetTextureBlendMode failed: " +
                                         std::string(SDL_GetError()));
            }

            // Set the render target to the new scaled texture
            if (SDL_SetRenderTarget(pRenderer, pScaledTexture) != 0) {
                SDL_DestroyTexture(pTexture);
                SDL_DestroyTexture(pScaledTexture);
                throw std::runtime_error("SDL_SetRenderTarget failed: " +
                                         std::string(SDL_GetError()));
            }

            // Set the renderer's draw color to transparent (0, 0, 0, 0)
            if (SDL_SetRenderDrawColor(pRenderer, 0, 0, 0, 0) != 0) {
                SDL_DestroyTexture(pTexture);
                SDL_DestroyTexture(pScaledTexture);
                throw std::runtime_error("SDL_SetRenderDrawColor failed: " +
                                         std::string(SDL_GetError()));
            }

            // Clear the render target (scaled texture) with transparent color
            if (SDL_RenderClear(pRenderer) != 0) {
                SDL_DestroyTexture(pTexture);
                SDL_DestroyTexture(pScaledTexture);
                throw std::runtime_error("SDL_RenderClear failed: " + std::string(SDL_GetError()));
            }

            // Enable alpha blending for the renderer
            if (SDL_SetRenderDrawBlendMode(pRenderer, SDL_BLENDMODE_BLEND) != 0) {
                SDL_DestroyTexture(pTexture);
                SDL_DestroyTexture(pScaledTexture);
                throw std::runtime_error("SDL_SetRenderDrawBlendMode failed: " +
                                         std::string(SDL_GetError()));
            }

            // Define source and destination rectangles for scaling
            SDL_Rect srcRect = {0, 0, 0, 0};
            SDL_Rect dstRect = {0, 0, newWidth, newHeight};

            // Get the width and height of the original texture
            if (SDL_QueryTexture(pTexture, nullptr, nullptr, &srcRect.w, &srcRect.h) != 0) {
                SDL_DestroyTexture(pTexture);
                SDL_DestroyTexture(pScaledTexture);
                throw std::runtime_error("SDL_QueryTexture failed: " + std::string(SDL_GetError()));
            }

            // Copy and scale the original texture to the new scaled texture
            if (SDL_RenderCopy(pRenderer, pTexture, &srcRect, &dstRect) != 0) {
                SDL_DestroyTexture(pTexture);
                SDL_DestroyTexture(pScaledTexture);
                throw std::runtime_error("SDL_RenderCopy failed: " + std::string(SDL_GetError()));
            }

            // Reset the render target to the default (usually the window)
            if (SDL_SetRenderTarget(pRenderer, nullptr) != 0) {
                SDL_DestroyTexture(pTexture);
                SDL_DestroyTexture(pScaledTexture);
                throw std::runtime_error("SDL_SetRenderTarget (reset) failed: " +
                                         std::string(SDL_GetError()));
            }

            // Destroy the original texture as it's no longer needed
            SDL_DestroyTexture(pTexture);

            // Store the scaled texture in the texture map
            m_textureMap[id] = pScaledTexture;
        } else {
            // If no scaling is needed, store the original texture
            m_textureMap[id] = pTexture;
        }
        return true;
    }

    return false;
}

void TextureManager::draw(std::string id, int x, int y, int width, int height,
                          SDL_Renderer* pRenderer, SDL_RendererFlip flip) {
    SDL_Rect srcRect;
    SDL_Rect destRect;

    srcRect.x = 0;
    srcRect.y = 0;
    srcRect.w = destRect.w = width;
    srcRect.h = destRect.h = height;
    destRect.x = x;
    destRect.y = y;

    SDL_RenderCopyEx(pRenderer, m_textureMap[id], &srcRect, &destRect, 0, 0, flip);
}

SDL_Texture* TextureManager::getTexture(std::string id) { return m_textureMap[id]; }
