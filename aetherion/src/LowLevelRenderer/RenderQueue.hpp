#ifndef RENDERQUEUE_H
#define RENDERQUEUE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/bind_map.h>
#include <nanobind/stl/bind_vector.h>
#include <nanobind/stl/string.h>

#include <algorithm>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "FontManager.hpp"
#include "TextureManager.hpp"

class RenderTaskBase {
   public:
    virtual ~RenderTaskBase() = default;

    // Pure virtual render method to be implemented by derived classes
    virtual void render(SDL_Renderer* renderer) const = 0;

    // Optional: Methods to set properties common to all tasks
    virtual void setZLayer(int z) { z_layer = z; }
    virtual int getZLayer() const { return z_layer; }

   protected:
    int z_layer;  // Z-order layer for rendering
};

class RenderTextureTask : public RenderTaskBase {
   public:
    RenderTextureTask(SDL_Texture* tex, int x_pos, int y_pos, float lightIntensity, float opacity)
        : texture(tex),
          x(x_pos),
          y(y_pos),
          lightIntensity(lightIntensity),
          opacity(opacity),
          use_source_rect(false) {}

    // Constructor for partial texture rendering
    RenderTextureTask(SDL_Texture* tex, int x_pos, int y_pos, float lightIntensity, float opacity,
                      int src_x, int src_y, int src_w, int src_h)
        : texture(tex),
          x(x_pos),
          y(y_pos),
          lightIntensity(lightIntensity),
          opacity(opacity),
          use_source_rect(true),
          source_rect{src_x, src_y, src_w, src_h} {}

    void render(SDL_Renderer* renderer) const override {
        if (!texture) {
            // Handle null texture appropriately
            SDL_Log("RenderTextureTask Error: Texture is null.");
            return;
        }

        SDL_Rect dst_rect;
        SDL_Rect* src_rect_ptr = nullptr;

        if (use_source_rect) {
            // Use partial texture rendering
            dst_rect.w = source_rect.w;
            dst_rect.h = source_rect.h;
            src_rect_ptr = const_cast<SDL_Rect*>(&source_rect);
        } else {
            // Use full texture rendering
            if (SDL_QueryTexture(texture, NULL, NULL, &dst_rect.w, &dst_rect.h) != 0) {
                SDL_Log("SDL_QueryTexture Error: %s", SDL_GetError());
                return;
            }
        }

        dst_rect.x = x;
        dst_rect.y = y;

        // Clamp light intensity between 0.0 and 1.0
        float clampedIntensity = std::max(0.0f, std::min(1.0f, lightIntensity));
        Uint8 lightColor = static_cast<Uint8>(255 * clampedIntensity);

        // Clamp opacity between 0.0 and 1.0
        float clampedOpacity = std::max(0.0f, std::min(1.0f, opacity));
        Uint8 alpha = static_cast<Uint8>(255 * clampedOpacity);

        // Store original color modulation and alpha
        Uint8 originalR, originalG, originalB, originalA;
        SDL_GetTextureColorMod(texture, &originalR, &originalG, &originalB);
        SDL_GetTextureAlphaMod(texture, &originalA);

        // Apply grayscale lighting by setting color modulation
        if (SDL_SetTextureColorMod(texture, lightColor, lightColor, lightColor) != 0) {
            SDL_Log("SDL_SetTextureColorMod Error: %s", SDL_GetError());
            // Handle error as needed
        }

        // Apply opacity by setting alpha modulation
        if (SDL_SetTextureAlphaMod(texture, alpha) != 0) {
            SDL_Log("SDL_SetTextureAlphaMod Error: %s", SDL_GetError());
            // Handle error as needed
        }

        // Render the texture (full or partial based on src_rect_ptr)
        if (SDL_RenderCopy(renderer, texture, src_rect_ptr, &dst_rect) != 0) {
            // Handle rendering error
            SDL_Log("SDL_RenderCopy Error: %s", SDL_GetError());
        }

        // Restore the original color modulation and alpha
        if (SDL_SetTextureColorMod(texture, originalR, originalG, originalB) != 0) {
            SDL_Log("SDL_SetTextureColorMod Restore Error: %s", SDL_GetError());
            // Handle error as needed
        }

        if (SDL_SetTextureAlphaMod(texture, originalA) != 0) {
            SDL_Log("SDL_SetTextureAlphaMod Restore Error: %s", SDL_GetError());
            // Handle error as needed
        }
    }

   private:
    SDL_Texture* texture;
    int x;
    int y;
    float lightIntensity;
    float opacity;
    bool use_source_rect;
    SDL_Rect source_rect;
};

class RenderRectTask : public RenderTaskBase {
   public:
    RenderRectTask(int x_pos, int y_pos, int width, int height, SDL_Color color)
        : rect_{x_pos, y_pos, width, height}, color_(color) {}

    void render(SDL_Renderer* renderer) const override {
        // Set the draw color
        if (SDL_SetRenderDrawColor(renderer, color_.r, color_.g, color_.b, color_.a) != 0) {
            SDL_Log("SDL_SetRenderDrawColor Error: %s", SDL_GetError());
            return;
        }

        // Render the filled rectangle
        if (SDL_RenderFillRect(renderer, &rect_) != 0) {
            SDL_Log("SDL_RenderFillRect Error: %s", SDL_GetError());
        }
    }

   private:
    SDL_Rect rect_;
    SDL_Color color_;
};

class RenderDrawRectTask : public RenderTaskBase {
   public:
    RenderDrawRectTask(int x_pos, int y_pos, int width, int height, int thickness, SDL_Color color)
        : rect_{x_pos, y_pos, width, height}, thickness_(thickness), color_(color) {}

    void render(SDL_Renderer* renderer) const override {
        // Set the draw color
        if (SDL_SetRenderDrawColor(renderer, color_.r, color_.g, color_.b, color_.a) != 0) {
            SDL_Log("SDL_SetRenderDrawColor Error: %s", SDL_GetError());
            return;
        }

        // Draw top border
        SDL_Rect topBorder = {rect_.x, rect_.y, rect_.w, thickness_};
        if (SDL_RenderFillRect(renderer, &topBorder) != 0) {
            SDL_Log("SDL_RenderFillRect Error: %s", SDL_GetError());
        }

        // Draw bottom border
        SDL_Rect bottomBorder = {rect_.x, rect_.y + rect_.h - thickness_, rect_.w, thickness_};
        if (SDL_RenderFillRect(renderer, &bottomBorder) != 0) {
            SDL_Log("SDL_RenderFillRect Error: %s", SDL_GetError());
        }

        // Draw left border
        SDL_Rect leftBorder = {rect_.x, rect_.y + thickness_, thickness_, rect_.h - 2 * thickness_};
        if (SDL_RenderFillRect(renderer, &leftBorder) != 0) {
            SDL_Log("SDL_RenderFillRect Error: %s", SDL_GetError());
        }

        // Draw right border
        SDL_Rect rightBorder = {rect_.x + rect_.w - thickness_, rect_.y + thickness_, thickness_,
                                rect_.h - 2 * thickness_};
        if (SDL_RenderFillRect(renderer, &rightBorder) != 0) {
            SDL_Log("SDL_RenderFillRect Error: %s", SDL_GetError());
        }
    }

   private:
    SDL_Rect rect_;
    int thickness_;
    SDL_Color color_;
};

class RenderLineTask : public RenderTaskBase {
   public:
    RenderLineTask(int x1, int y1, int x2, int y2, SDL_Color color)
        : x_start(x1), y_start(y1), x_end(x2), y_end(y2), color_(color) {}

    void render(SDL_Renderer* renderer) const override {
        // Set the draw color
        if (SDL_SetRenderDrawColor(renderer, color_.r, color_.g, color_.b, color_.a) != 0) {
            SDL_Log("SDL_SetRenderDrawColor Error: %s", SDL_GetError());
            return;
        }

        // Render the line
        if (SDL_RenderDrawLine(renderer, x_start, y_start, x_end, y_end) != 0) {
            SDL_Log("SDL_RenderDrawLine Error: %s", SDL_GetError());
        }
    }

   private:
    int x_start;
    int y_start;
    int x_end;
    int y_end;
    SDL_Color color_;
};

class RenderTextTask : public RenderTaskBase {
   public:
    RenderTextTask(const std::string& text, const std::string& font_id, SDL_Color color, int x_pos,
                   int y_pos)
        : text_(text), font_id_(font_id), color_(color), x_(x_pos), y_(y_pos) {}

    void render(SDL_Renderer* renderer) const override {
        TTF_Font* font = FontManager::Instance()->getFont(font_id_);
        if (!font) {
            SDL_Log("RenderTextTask Error: Font '%s' not found.", font_id_.c_str());
            return;
        }

        // Render text to an SDL_Surface
        SDL_Surface* text_surface = TTF_RenderUTF8_Blended(font, text_.c_str(), color_);
        if (!text_surface) {
            SDL_Log("TTF_RenderUTF8_Blended Error: %s", TTF_GetError());
            return;
        }

        // Convert SDL_Surface to SDL_Texture
        SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);
        if (!text_texture) {
            SDL_Log("SDL_CreateTextureFromSurface Error: %s", SDL_GetError());
            SDL_FreeSurface(text_surface);
            return;
        }

        // Set the destination rectangle
        SDL_Rect dst_rect = {x_, y_, text_surface->w, text_surface->h};

        // Render the texture
        if (SDL_RenderCopy(renderer, text_texture, NULL, &dst_rect) != 0) {
            SDL_Log("SDL_RenderCopy Error: %s", SDL_GetError());
        }

        // Clean up
        SDL_DestroyTexture(text_texture);
        SDL_FreeSurface(text_surface);
    }

   private:
    std::string text_;
    std::string font_id_;
    SDL_Color color_;
    int x_;
    int y_;
};

class RenderQueue {
   public:
    RenderQueue() {
        // Initialize with default priority order
        priority_order = {{"background", 0}, {"entities", 1}, {"effects", 2}, {"foreground", 3}};

        // Load font
        if (!FontManager::Instance()->loadFont("my_font", "resources/Toriko.ttf", 24)) {
            // Handle font loading error
        }
    }

    // Add RenderTextureTask by Texture ID (string)
    void add_task_by_id(int z_layer, const std::string& priority_group,
                        const std::string& texture_id, int x, int y, float lightIntensity,
                        float opacity) {
        SDL_Texture* tex = TextureManager::Instance()->getTexture(texture_id);
        if (!tex) {
            std::cerr << "Warning: Texture ID '" << texture_id << "' not found. Task skipped."
                      << std::endl;
            return;
        }
        add_task_internal(z_layer, priority_group,
                          std::make_shared<RenderTextureTask>(tex, x, y, lightIntensity, opacity));
    }

    // Add RenderTextureTask by SDL_Texture* (uintptr_t for safe casting)
    void add_task_by_texture(int z_layer, const std::string& priority_group, uintptr_t texture_ptr,
                             int x, int y, float lightIntensity, float opacity) {
        SDL_Texture* texture = reinterpret_cast<SDL_Texture*>(texture_ptr);
        if (!texture) {
            std::cerr << "Warning: Null texture provided. Task skipped." << std::endl;
            return;
        }
        add_task_internal(
            z_layer, priority_group,
            std::make_shared<RenderTextureTask>(texture, x, y, lightIntensity, opacity));
    }

    // Add RenderTextureTask with partial texture rendering by Texture ID (string)
    void add_task_by_id_partial(int z_layer, const std::string& priority_group,
                                const std::string& texture_id, int x, int y, float lightIntensity,
                                float opacity, int src_x, int src_y, int src_w, int src_h) {
        SDL_Texture* tex = TextureManager::Instance()->getTexture(texture_id);
        if (!tex) {
            std::cerr << "Warning: Texture ID '" << texture_id << "' not found. Task skipped."
                      << std::endl;
            return;
        }
        add_task_internal(z_layer, priority_group,
                          std::make_shared<RenderTextureTask>(tex, x, y, lightIntensity, opacity,
                                                              src_x, src_y, src_w, src_h));
    }

    // Add RenderTextureTask with partial texture rendering by SDL_Texture* (uintptr_t for safe
    // casting)
    void add_task_by_texture_partial(int z_layer, const std::string& priority_group,
                                     uintptr_t texture_ptr, int x, int y, float lightIntensity,
                                     float opacity, int src_x, int src_y, int src_w, int src_h) {
        SDL_Texture* texture = reinterpret_cast<SDL_Texture*>(texture_ptr);
        if (!texture) {
            std::cerr << "Warning: Null texture provided. Task skipped." << std::endl;
            return;
        }
        add_task_internal(z_layer, priority_group,
                          std::make_shared<RenderTextureTask>(texture, x, y, lightIntensity,
                                                              opacity, src_x, src_y, src_w, src_h));
    }

    // Convenience methods for common partial texture rendering scenarios

    // Render a quadrant of a texture (useful for 32x32 sprites split into 16x16 quadrants)
    enum class TextureQuadrant { TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT };

    void add_task_by_id_quadrant(int z_layer, const std::string& priority_group,
                                 const std::string& texture_id, int x, int y, float lightIntensity,
                                 float opacity, TextureQuadrant quadrant) {
        SDL_Texture* tex = TextureManager::Instance()->getTexture(texture_id);
        if (!tex) {
            std::cerr << "Warning: Texture ID '" << texture_id << "' not found. Task skipped."
                      << std::endl;
            return;
        }

        // Get texture dimensions
        int tex_width, tex_height;
        if (SDL_QueryTexture(tex, NULL, NULL, &tex_width, &tex_height) != 0) {
            std::cerr << "Warning: Could not query texture dimensions. Task skipped." << std::endl;
            return;
        }

        int half_width = tex_width / 2;
        int half_height = tex_height / 2;
        int src_x = 0, src_y = 0;

        switch (quadrant) {
            case TextureQuadrant::TOP_LEFT:
                src_x = 0;
                src_y = 0;
                break;
            case TextureQuadrant::TOP_RIGHT:
                src_x = half_width;
                src_y = 0;
                break;
            case TextureQuadrant::BOTTOM_LEFT:
                src_x = 0;
                src_y = half_height;
                break;
            case TextureQuadrant::BOTTOM_RIGHT:
                src_x = half_width;
                src_y = half_height;
                break;
        }

        add_task_internal(
            z_layer, priority_group,
            std::make_shared<RenderTextureTask>(tex, x, y, lightIntensity, opacity, src_x, src_y,
                                                half_width, half_height));
    }

    // Render a custom fraction of a texture (e.g., left half, top third, etc.)
    void add_task_by_id_fraction(int z_layer, const std::string& priority_group,
                                 const std::string& texture_id, int x, int y, float lightIntensity,
                                 float opacity, float x_start_ratio, float y_start_ratio,
                                 float width_ratio, float height_ratio) {
        SDL_Texture* tex = TextureManager::Instance()->getTexture(texture_id);
        if (!tex) {
            std::cerr << "Warning: Texture ID '" << texture_id << "' not found. Task skipped."
                      << std::endl;
            return;
        }

        // Get texture dimensions
        int tex_width, tex_height;
        if (SDL_QueryTexture(tex, NULL, NULL, &tex_width, &tex_height) != 0) {
            std::cerr << "Warning: Could not query texture dimensions. Task skipped." << std::endl;
            return;
        }

        // Calculate source rectangle based on ratios
        int src_x = static_cast<int>(tex_width * std::max(0.0f, std::min(1.0f, x_start_ratio)));
        int src_y = static_cast<int>(tex_height * std::max(0.0f, std::min(1.0f, y_start_ratio)));
        int src_w = static_cast<int>(tex_width * std::max(0.0f, std::min(1.0f, width_ratio)));
        int src_h = static_cast<int>(tex_height * std::max(0.0f, std::min(1.0f, height_ratio)));

        // Ensure we don't go beyond texture bounds
        src_w = std::min(src_w, tex_width - src_x);
        src_h = std::min(src_h, tex_height - src_y);

        add_task_internal(z_layer, priority_group,
                          std::make_shared<RenderTextureTask>(tex, x, y, lightIntensity, opacity,
                                                              src_x, src_y, src_w, src_h));
    }

    // Add RenderRectTask
    void add_task_rect(int z_layer, const std::string& priority_group, int x, int y, int width,
                       int height, SDL_Color color) {
        add_task_internal(z_layer, priority_group,
                          std::make_shared<RenderRectTask>(x, y, width, height, color));
    }

    // Add RenderRectTask
    void add_task_draw_rect(int z_layer, const std::string& priority_group, int x, int y, int width,
                            int height, int thickness, SDL_Color color) {
        add_task_internal(
            z_layer, priority_group,
            std::make_shared<RenderDrawRectTask>(x, y, width, height, thickness, color));
    }

    // Add RenderLineTask
    void add_task_line(int z_layer, const std::string& priority_group, int x1, int y1, int x2,
                       int y2, SDL_Color color) {
        add_task_internal(z_layer, priority_group,
                          std::make_shared<RenderLineTask>(x1, y1, x2, y2, color));
    }

    // Add RenderTextTask
    void add_task_text(int z_layer, const std::string& priority_group, const std::string& text,
                       const std::string& font_id, SDL_Color color, int x, int y) {
        add_task_internal(z_layer, priority_group,
                          std::make_shared<RenderTextTask>(text, font_id, color, x, y));
    }

    // End of methods to handle other task types.

    void clear() {
        std::lock_guard<std::mutex> lock(queue_mutex);
        queue.clear();
    }

    std::vector<int> get_sorted_layers() const {
        std::lock_guard<std::mutex> lock(queue_mutex);
        std::vector<int> layers;
        for (const auto& pair : queue) {
            layers.push_back(pair.first);
        }
        std::sort(layers.begin(), layers.end());
        return layers;
    }

    int get_priority_order_value(const std::string& priority_group) const {
        auto it = priority_order.find(priority_group);
        if (it != priority_order.end()) {
            return it->second;
        }
        return 99;  // Default low priority
    }

    std::vector<std::string> get_sorted_priority_groups(
        const std::vector<std::string>& groups) const {
        std::vector<std::string> sorted_groups = groups;
        std::sort(sorted_groups.begin(), sorted_groups.end(),
                  [&](const std::string& a, const std::string& b) -> bool {
                      return get_priority_order_value(a) < get_priority_order_value(b);
                  });
        return sorted_groups;
    }

    void set_priority_order(const std::map<std::string, int>& new_priority_order) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        priority_order = new_priority_order;
    }

    // Rendering method
    void render(uintptr_t renderer_ptr) const {
        SDL_Renderer* renderer = reinterpret_cast<SDL_Renderer*>(renderer_ptr);
        std::lock_guard<std::mutex> lock(queue_mutex);
        const auto& internal_queue = queue;

        // Extract sorted layers
        std::vector<int> sorted_layers;
        for (const auto& pair : internal_queue) {
            sorted_layers.push_back(pair.first);
        }
        std::sort(sorted_layers.begin(), sorted_layers.end());

        for (const auto& layer : sorted_layers) {
            const auto& priority_map = internal_queue.at(layer);

            // Extract and sort priority groups
            std::vector<std::string> priority_groups;
            for (const auto& pair : priority_map) {
                priority_groups.push_back(pair.first);
            }

            // Sort priority groups based on priority_order
            std::vector<std::string> sorted_priority_groups =
                get_sorted_priority_groups(priority_groups);

            for (const auto& group : sorted_priority_groups) {
                const auto& tasks = priority_map.at(group);

                // Optionally, implement batching or other optimizations here

                // Normal order
                for (const auto& task : tasks) {
                    task->render(renderer);
                }
                // Reversed order
                // for (auto it = tasks.rbegin(); it != tasks.rend(); ++it) {
                //     (*it)->render(renderer);
                // }
            }
        }
    }

   private:
    // Structure: { z_layer: { priority_group: [RenderTaskBase, ...] } }
    std::map<int, std::map<std::string, std::vector<std::shared_ptr<RenderTaskBase>>>> queue;

    // Priority mapping: priority_group -> priority_value
    std::map<std::string, int> priority_order;

    mutable std::mutex queue_mutex;

    // Internal method to add tasks
    void add_task_internal(int z_layer, const std::string& priority_group,
                           std::shared_ptr<RenderTaskBase> task) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        queue[z_layer][priority_group].push_back(task);
    }
};

#endif  // RENDERQUEUE_H