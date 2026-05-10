#include "ui/GameUI.hpp"

#include <format>
#include <iostream>
#include <cctype>
#include <map>

#include <GLFW/glfw3.h>
#include <RmlUi/Core.h>
#include "RmlUi_Platform_GLFW.h"
#include "RmlUi_Renderer_GL2.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl2.h>

namespace voxel {
namespace {

std::string displayNameForRecipeType(const std::string& type) {
    if (type == "crafting") return "Crafting";
    if (type == "smelting") return "Smelting";
    if (type.empty()) return "Other";

    std::string label = type;
    label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
    for (std::size_t i = 1; i < label.size(); ++i) {
        if (label[i - 1] == '_' || label[i - 1] == '-' || label[i - 1] == ' ') {
            label[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[i])));
        }
    }
    return label;
}

std::string displayNameForItemOrBlock(const GameData* gameData, const std::string& id) {
    if (gameData != nullptr) {
        if (const auto itemIt = gameData->items.find(id); itemIt != gameData->items.end()) {
            return itemIt->second.name;
        }
        if (const auto blockIt = gameData->blocks.find(id); blockIt != gameData->blocks.end()) {
            return blockIt->second.name;
        }
    }
    return id;
}

}  // namespace

// ── Constructor ───────────────────────────────────────────────────────────────

GameUI::GameUI(GLFWwindow* window, int width, int height, const std::string& assetsRoot)
    : window_(window), assetsRoot_(assetsRoot), width_(width), height_(height)
{
    // ── RmlUI ──────────────────────────────────────────────────────────────────
    rmlSystem_.SetWindow(window_);
    Rml::SetSystemInterface(&rmlSystem_);
    Rml::SetRenderInterface(&rmlRenderer_);
    Rml::Initialise();

    rmlRenderer_.SetViewport(width_, height_);

    const std::string fontPath = assetsRoot_ + "/ui/fonts/LatoLatin-Regular.ttf";
    if (!Rml::LoadFontFace(fontPath))
        std::cerr << "[GameUI] Warning: failed to load font: " << fontPath << '\n';

    float dpRatio = 1.0f;
    glfwGetWindowContentScale(window_, &dpRatio, nullptr);

    // Context dimensions in framebuffer (physical) pixels; DPR scales dp→px correctly
    rmlContext_ = Rml::CreateContext("main", Rml::Vector2i(width_, height_));
    if (!rmlContext_) {
        std::cerr << "[GameUI] Failed to create RmlUI context\n";
    } else {
        const std::string hudPath = assetsRoot_ + "/ui/hud.rml";
        hudDoc_ = rmlContext_->LoadDocument(hudPath);
        if (!hudDoc_)
            std::cerr << "[GameUI] Failed to load: " << hudPath << '\n';
        else
            rmlContext_->SetDensityIndependentPixelRatio(dpRatio);  // 1dp = dpRatio fb-pixels
        hudDoc_->Show();
    }

    // ── ImGui ──────────────────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;  // Don't persist layout
    ImGui_ImplGlfw_InitForOpenGL(window_, false);  // false = don't install callbacks
    ImGui_ImplOpenGL2_Init();
    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowBorderSize = 0.0f;
    ImGui::GetStyle().WindowRounding   = 5.0f;
}

// ── Destructor ────────────────────────────────────────────────────────────────

GameUI::~GameUI() {
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (rmlContext_) Rml::RemoveContext("main");
    Rml::Shutdown();
}

// ── update ────────────────────────────────────────────────────────────────────

void GameUI::update() {
    if (rmlContext_) rmlContext_->Update();

    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    const auto& d = debugData_;
    ImGui::SetNextWindowPos({10.0f, 10.0f});
    ImGui::SetNextWindowBgAlpha(0.6f);
    ImGui::Begin("##debug", nullptr,
        ImGuiWindowFlags_NoDecoration  |
        ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("POS    %.1f  %.1f  %.1f",
        d.playerPosition.x, d.playerPosition.y, d.playerPosition.z);
    if (d.targetedBlock.has_value())
        ImGui::Text("TARGET %d  %d  %d",
            d.targetedBlock->x, d.targetedBlock->y, d.targetedBlock->z);
    else
        ImGui::Text("TARGET -");
    ImGui::Separator();
    ImGui::Text("FPS    %d  (%.2f ms)", d.fps, d.frameTimeMs);
    ImGui::Text("CHUNK  %d  %d  %d", d.chunkX, d.chunkY, d.chunkZ);
    ImGui::Text("CHUNKS %d", d.loadedChunks);
    ImGui::Text("SOLID  %d", d.solidBlocks);
    ImGui::Text("FACES  %d", d.visibleFaces);
    ImGui::Text("TRIS   %d", d.triangleCount);
    ImGui::Separator();
    ImGui::Text("BIOME  %s", d.biomeName.empty() ? "-" : d.biomeName.c_str());
    ImGui::Text("TEMP   %.2f", d.temperature);
    ImGui::Text("HUM    %.2f", d.humidity);
    ImGui::Text("RAIN   %.2f", d.rainfall);
    ImGui::Text("ELEV   %.2f", d.elevation);
    ImGui::Text("DRAIN  %.2f", d.drainage);
    ImGui::Text("WTBL   %.2f", d.waterTable);

    ImGui::End();

    // Chat Window
    ImGui::SetNextWindowPos({10.0f, (float)height_ - 300.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({400.0f, 250.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.4f);

    ImGuiWindowFlags chatFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;
    if (!chatOpen_) chatFlags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground;

    if (ImGui::Begin("##chat", nullptr, chatFlags)) {
        // Log Area
        ImGui::BeginChild("##chat_log", {0, chatOpen_ ? -30.0f : 0.0f}, false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        for (const auto& msg : chatMessages_) {
            ImGui::TextWrapped("%s: %s", msg.sender.c_str(), msg.text.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();

        if (chatOpen_) {
            ImGui::Separator();
            ImGui::SetKeyboardFocusHere();
            const bool enterDown = glfwGetKey(window_, GLFW_KEY_ENTER) == GLFW_PRESS;
            if (!chatSubmitArmed_ && !enterDown) {
                chatSubmitArmed_ = true;
            }
            ImGui::InputText("##chat_input", chatInputBuffer_, sizeof(chatInputBuffer_));
            if (chatSubmitArmed_ && enterDown && !chatEnterWasDown_) {
                pendingChatInput_ = chatInputBuffer_;
                chatInputSubmitted_ = true;
                chatSubmitArmed_ = false;
                std::memset(chatInputBuffer_, 0, sizeof(chatInputBuffer_));
            }
            chatEnterWasDown_ = enterDown;
        }
    }
    ImGui::End();

    // Crafting Window
    if (craftingOpen_) {
        ImGui::SetNextWindowPos({(float)width_ / 2.0f, (float)height_ / 2.0f}, ImGuiCond_Appearing, {0.5f, 0.5f});
        ImGui::SetNextWindowSize({500.0f, 400.0f}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Crafting", &craftingOpen_)) {
            if (recipes_.empty()) {
                ImGui::Text("No recipes available.");
            } else {
                std::map<std::string, std::vector<const RecipeDefinition*>> recipesByType;
                for (const auto& recipe : recipes_) {
                    recipesByType[recipe.type.empty() ? "other" : recipe.type].push_back(&recipe);
                }

                if (ImGui::BeginTabBar("##recipe_type_tabs")) {
                    for (const auto& [type, typedRecipes] : recipesByType) {
                        const std::string tabLabel = std::format("{} ({})", displayNameForRecipeType(type), typedRecipes.size());
                        if (!ImGui::BeginTabItem(tabLabel.c_str())) {
                            continue;
                        }

                        for (const RecipeDefinition* recipe : typedRecipes) {
                            ImGui::PushID(recipe->id.c_str());

                            const std::string outputName = displayNameForItemOrBlock(gameData_, recipe->output);
                            if (ImGui::CollapsingHeader(std::format("{} x{}", outputName, recipe->count).c_str())) {
                                bool canCraft = true;
                                ImGui::Text("Ingredients:");
                                for (const auto& ingredient : recipe->ingredients) {
                                    const std::string ingName = displayNameForItemOrBlock(gameData_, ingredient);
                                    const bool has = hasItem(inventory_, ingredient, 1);
                                    if (!has) canCraft = false;

                                    ImGui::BulletText("%s %s", ingName.c_str(), has ? "(OK)" : "(Missing)");
                                }

                                ImGui::Separator();
                                if (!canCraft) ImGui::BeginDisabled();
                                if (ImGui::Button("Craft")) {
                                    pendingCraftRequest_ = recipe->id;
                                }
                                if (!canCraft) ImGui::EndDisabled();
                            }
                            ImGui::PopID();
                        }

                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
            }
        }
        ImGui::End();
    }
}

// ── render ────────────────────────────────────────────────────────────────────

void GameUI::render() {
    if (rmlContext_) {
        rmlRenderer_.BeginFrame();
        rmlContext_->Render();
        rmlRenderer_.EndFrame();
    }

    ImGui::Render();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}

// ── GLFW callbacks ────────────────────────────────────────────────────────────

void GameUI::onFramebufferSize(int width, int height) {
    width_  = width;
    height_ = height;
    rmlRenderer_.SetViewport(width, height);
    if (rmlContext_)
        rmlContext_->SetDimensions(Rml::Vector2i(width, height));
}

void GameUI::onContentScale(float /*xscale*/, float /*yscale*/) {}

void GameUI::onKey(int key, int scancode, int action, int mods) {
    ImGui_ImplGlfw_KeyCallback(window_, key, scancode, action, mods);
    if (ImGui::GetIO().WantCaptureKeyboard) return;

    if (rmlContext_)
        RmlGLFW::ProcessKeyCallback(rmlContext_, key, action, mods);
}

void GameUI::onChar(unsigned int codepoint) {
    ImGui_ImplGlfw_CharCallback(window_, codepoint);
    if (ImGui::GetIO().WantTextInput) return;

    if (rmlContext_)
        RmlGLFW::ProcessCharCallback(rmlContext_, codepoint);
}

void GameUI::onCursorPos(double x, double y) {
    ImGui_ImplGlfw_CursorPosCallback(window_, x, y);
    if (ImGui::GetIO().WantCaptureMouse) return;

    if (rmlContext_)
        RmlGLFW::ProcessCursorPosCallback(rmlContext_, window_, x, y, 0);
}

void GameUI::onMouseButton(int button, int action, int mods) {
    ImGui_ImplGlfw_MouseButtonCallback(window_, button, action, mods);
    if (ImGui::GetIO().WantCaptureMouse) return;

    if (rmlContext_)
        RmlGLFW::ProcessMouseButtonCallback(rmlContext_, button, action, mods);
}

void GameUI::onScroll(double xoffset, double yoffset) {
    ImGui_ImplGlfw_ScrollCallback(window_, xoffset, yoffset);
    if (ImGui::GetIO().WantCaptureMouse) return;

    if (rmlContext_)
        RmlGLFW::ProcessScrollCallback(rmlContext_, yoffset, 0);
}

// ── HUD helpers ───────────────────────────────────────────────────────────────

void GameUI::setDebugData(const DebugOverlayData& data) {
    debugData_ = data;
}

void GameUI::setInventory(const Inventory& inventory) {
    inventory_ = inventory;
    if (!hudDoc_) return;

    for (int i = 0; i < kInventorySlots; ++i) {
        const std::string bgId = "slot-" + std::to_string(i) + "-bg";
        if (auto* el = hudDoc_->GetElementById(bgId))
            el->SetClass("selected", i == inventory.selectedIndex);

        const std::string countId = "slot-" + std::to_string(i) + "-count";
        if (auto* el = hudDoc_->GetElementById(countId)) {
            const auto& slot = inventory.slots[static_cast<std::size_t>(i)];
            el->SetInnerRML(slot.count > 0 ? std::to_string(slot.count) : "");
        }
    }
}

void GameUI::setElementText(const char* id, const std::string& text) {
    if (!hudDoc_) return;
    if (auto* el = hudDoc_->GetElementById(id))
        el->SetInnerRML(text);
}

void GameUI::setElementClass(const char* id, const char* cls, bool enable) {
    if (!hudDoc_) return;
    if (auto* el = hudDoc_->GetElementById(id))
        el->SetClass(cls, enable);
}

void GameUI::addChatMessage(const std::string& sender, const std::string& text) {
    chatMessages_.push_back({sender, text});
    if (chatMessages_.size() > 100) {
        chatMessages_.erase(chatMessages_.begin());
    }
}

void GameUI::setChatOpen(bool open) {
    chatOpen_ = open;
    if (chatOpen_) {
        const bool enterDown = glfwGetKey(window_, GLFW_KEY_ENTER) == GLFW_PRESS;
        std::memset(chatInputBuffer_, 0, sizeof(chatInputBuffer_));
        chatInputSubmitted_ = false;
        pendingChatInput_.clear();
        chatSubmitArmed_ = !enterDown;
        chatEnterWasDown_ = enterDown;
    } else {
        chatSubmitArmed_ = true;
        chatEnterWasDown_ = false;
        std::memset(chatInputBuffer_, 0, sizeof(chatInputBuffer_));
        pendingChatInput_.clear();
    }
}

void GameUI::setRecipes(const std::vector<RecipeDefinition>& recipes) {
    recipes_ = recipes;
}

std::string GameUI::consumePendingChatInput() {
    if (chatInputSubmitted_) {
        chatInputSubmitted_ = false;
        std::string input = std::move(pendingChatInput_);
        pendingChatInput_.clear();
        return input;
    }
    return "";
}

std::string GameUI::consumePendingCraftRequest() {
    std::string req = std::move(pendingCraftRequest_);
    pendingCraftRequest_.clear();
    return req;
}

}  // namespace voxel
