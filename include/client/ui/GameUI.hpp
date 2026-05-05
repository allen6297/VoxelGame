#pragma once

#include <string>

#include <RmlUi/Core.h>
#include "RmlUi_Platform_GLFW.h"
#include "RmlUi_Renderer_GL2.h"

#include "player/Inventory.hpp"
#include "render/Renderer.hpp"

struct GLFWwindow;

namespace voxel {

// Owns the RmlUI context (permanent game HUD) and ImGui debug overlay.
// Create after the GL context is current.  Call render() after all 3D drawing.
class GameUI {
public:
    GameUI(GLFWwindow* window, int width, int height, const std::string& assetsRoot);
    ~GameUI();

    // Call once per frame before render()
    void update();

    // Call after all 3D rendering — draws UI on top
    void render();

    // Forward GLFW callbacks here from main
    void onFramebufferSize(int width, int height);
    void onContentScale(float xscale, float yscale);
    void onKey(int key, int scancode, int action, int mods);
    void onChar(unsigned int codepoint);
    void onCursorPos(double x, double y);
    void onMouseButton(int button, int action, int mods);
    void onScroll(double xoffset, double yoffset);

    // Push game state into the HUD every frame
    void setDebugData(const DebugOverlayData& data);
    void setInventory(const Inventory& inventory);
    void setRecipes(const std::vector<RecipeDefinition>& recipes);
    void setGameData(const GameData* gameData) { gameData_ = gameData; }

    struct ChatMessage {
        std::string sender;
        std::string text;
    };
    void addChatMessage(const std::string& sender, const std::string& text);
    bool isChatOpen() const { return chatOpen_; }
    void setChatOpen(bool open);
    bool isCraftingOpen() const { return craftingOpen_; }
    void setCraftingOpen(bool open) { craftingOpen_ = open; }
    std::string consumePendingChatInput();
    std::string consumePendingCraftRequest();

private:
    GLFWwindow* window_    = nullptr;
    std::string assetsRoot_;
    int         width_     = 0;
    int         height_    = 0;

    // RmlUI (permanent HUD: hotbar, health bar)
    SystemInterface_GLFW                  rmlSystem_;
    RenderInterface_GL2   rmlRenderer_;
    Rml::Context*                         rmlContext_ = nullptr;
    Rml::ElementDocument*                 hudDoc_     = nullptr;

    // Debug data for ImGui overlay (updated each frame via setDebugData)
    DebugOverlayData debugData_{};

    // Chat
    std::vector<ChatMessage> chatMessages_;
    bool chatOpen_ = false;
    char chatInputBuffer_[256] = {0};
    bool chatInputSubmitted_ = false;

    // Crafting
    bool craftingOpen_ = false;
    std::vector<RecipeDefinition> recipes_;
    Inventory inventory_;
    const GameData* gameData_ = nullptr;
    std::string pendingCraftRequest_;

    // RML element helpers
    void setElementText(const char* id, const std::string& text);
    void setElementClass(const char* id, const char* cls, bool enable);
};

}  // namespace voxel
