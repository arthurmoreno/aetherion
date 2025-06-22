#include "TextureManager.hpp"

#include <SDL2/SDL_opengl.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>

// Simplified OpenGL texture rendering without modern pipeline
// Uses basic OpenGL 1.1 functionality available in SDL2

// Initialize static OpenGL members (simplified for OpenGL 1.1)
std::map<std::string, GLuint> TextureManager::s_textureMapGL;
bool TextureManager::s_openglInitialized = false;

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

// -------------------
// Work in progress  |
// -------------------

// OpenGL TextureManager methods (OpenGL 1.1 compatible)
void TextureManager::initOpenGL() {
    if (s_openglInitialized) return;

    // Enable 2D texturing and basic OpenGL features for texture rendering
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    s_openglInitialized = true;
    std::cout << "OpenGL TextureManager initialized successfully (OpenGL 1.1)" << std::endl;
}

void TextureManager::cleanupOpenGL() {
    if (!s_openglInitialized) return;

    // Clean up textures
    for (auto& pair : s_textureMapGL) {
        glDeleteTextures(1, &pair.second);
    }
    s_textureMapGL.clear();

    s_openglInitialized = false;
    std::cout << "OpenGL TextureManager cleaned up" << std::endl;
}

bool TextureManager::loadGL(std::string fileName, std::string id, uintptr_t gl_context_ptr,
                            int newWidth, int newHeight) {
    // Initialize OpenGL if not already done
    if (!s_openglInitialized) {
        initOpenGL();
    }

    // Load image surface using SDL2_image (same as SDL version)
    SDL_Surface* surface = IMG_Load(fileName.c_str());
    if (!surface) {
        std::cerr << "Failed to load image: " << IMG_GetError() << std::endl;
        return false;
    }

    // Convert surface to RGBA format for OpenGL
    SDL_Surface* rgbaSurface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surface);

    if (!rgbaSurface) {
        std::cerr << "Failed to convert surface to RGBA: " << SDL_GetError() << std::endl;
        return false;
    }

    // Generate OpenGL texture
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload texture data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgbaSurface->w, rgbaSurface->h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, rgbaSurface->pixels);

    SDL_FreeSurface(rgbaSurface);

    // Store in texture map
    s_textureMapGL[id] = texture;

    std::cout << "OpenGL texture '" << id << "' loaded successfully (ID: " << texture << ")"
              << std::endl;
    return true;
}

void TextureManager::renderGL(std::string id, int x, int y, int width, int height) {
    if (!s_openglInitialized) {
        initOpenGL();
    }

    auto it = s_textureMapGL.find(id);
    if (it == s_textureMapGL.end()) {
        std::cerr << "OpenGL texture '" << id << "' not found" << std::endl;
        return;
    }

    GLuint texture = it->second;

    // Use default size if not specified
    if (width <= 0 || height <= 0) {
        width = 100;  // Default size
        height = 100;
    }

    // Save current OpenGL state
    glPushMatrix();
    glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT);

    // Enable texturing
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Set up orthographic projection for 2D rendering
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    // Get viewport to set up coordinate system
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glOrtho(0, viewport[2], viewport[3], 0, -1, 1);  // Top-left origin, like screen coordinates

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Render textured quad using immediate mode
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(x, y);  // Top-left
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(x + width, y);  // Top-right
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(x + width, y + height);  // Bottom-right
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(x, y + height);  // Bottom-left
    glEnd();

    // Restore OpenGL state
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glPopAttrib();
    glPopMatrix();
}

GLuint TextureManager::getTextureGL(std::string id) {
    auto it = s_textureMapGL.find(id);
    if (it != s_textureMapGL.end()) {
        return it->second;
    }
    return 0;
}

// OpenGL C-style wrapper functions
GLuint load_texture_gl(uintptr_t gl_context_ptr, const std::string& image_path) {
    // Initialize OpenGL if not already done
    if (!TextureManager::s_openglInitialized) {
        TextureManager::initOpenGL();
    }

    // Load image surface
    SDL_Surface* surface = IMG_Load(image_path.c_str());
    if (!surface) {
        throw std::runtime_error(std::string("Failed to load image: ") + IMG_GetError());
    }

    // Convert to RGBA format
    SDL_Surface* rgbaSurface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surface);

    if (!rgbaSurface) {
        throw std::runtime_error(std::string("Failed to convert surface to RGBA: ") +
                                 SDL_GetError());
    }

    // Generate OpenGL texture
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload texture data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgbaSurface->w, rgbaSurface->h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, rgbaSurface->pixels);

    SDL_FreeSurface(rgbaSurface);

    return texture;
}

void render_texture_gl(uintptr_t gl_context_ptr, GLuint texture_id, int x, int y, int width,
                       int height) {
    if (!TextureManager::s_openglInitialized) {
        TextureManager::initOpenGL();
    }

    // Use default size if not specified
    if (width <= 0 || height <= 0) {
        width = 100;
        height = 100;
    }

    // Save current OpenGL state
    glPushMatrix();
    glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT);

    // Enable texturing
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    // Set up orthographic projection for 2D rendering
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    // Get viewport to set up coordinate system
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glOrtho(0, viewport[2], viewport[3], 0, -1, 1);  // Top-left origin

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Render textured quad using immediate mode
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(x, y);  // Top-left
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(x + width, y);  // Top-right
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(x + width, y + height);  // Bottom-right
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(x, y + height);  // Bottom-left
    glEnd();

    // Restore OpenGL state
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glPopAttrib();
    glPopMatrix();
}

void destroy_texture_gl(GLuint texture_id) { glDeleteTextures(1, &texture_id); }

void loadTextureOnManagerGL(uintptr_t gl_context_ptr, const std::string& imagePath,
                            const std::string& id, int newWidth, int newHeight) {
    TextureManager::Instance()->loadGL(imagePath, id, gl_context_ptr, newWidth, newHeight);
}

void renderTextureFromManagerGL(uintptr_t gl_context_ptr, const std::string& id, int x, int y,
                                int width, int height) {
    TextureManager::Instance()->renderGL(id, x, y, width, height);
}

GLuint getTextureFromManagerGL(const std::string& id) {
    return TextureManager::Instance()->getTextureGL(id);
}
