// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_IMGUI_IMGUI_INSTANCE_H_
#define GB_IMGUI_IMGUI_INSTANCE_H_

#include "gb/base/validated_context.h"
#include "gb/file/file_types.h"
#include "gb/imgui/imgui_types.h"
#include "gb/render/render_types.h"
#include "gb/resource/resource_set.h"
#include "imgui.h"

namespace gb {

// This class manages an ImGuiContext,
class ImGuiInstance final {
 public:
  //----------------------------------------------------------------------------
  // Contract constraints
  //----------------------------------------------------------------------------

  // REQUIRED: FileSystem interface. The file system is used to as needed by
  // ImGui (for instance, to load fonts or write configuration files).
  static GB_CONTEXT_CONSTRAINT(kConstraintFileSystem, kInRequired, FileSystem);

  // REQUIRED: RenderSystem interface. This is used to draw the GUI.
  static GB_CONTEXT_CONSTRAINT(kConstraintRenderSystem, kInRequired,
                               RenderSystem);

  // OPTIONAL: Scene order for the GUI scene. By default this is 100, which is
  // suitably late.
  static inline constexpr char* kKeySceneOrder = "SceneOrder";
  static GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(kConstraintSceneOrder, kInOptional,
                                             int, kKeySceneOrder, 100);

  // SCOPED: The ImGuiContext that this instance is managing.
  static GB_CONTEXT_CONSTRAINT(kConstraintImGuiContext, kScoped, ImGuiContext);

  using CreateContract =
      ContextContract<kConstraintFileSystem, kConstraintRenderSystem,
                      kConstraintSceneOrder, kConstraintImGuiContext>;

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  ImGuiInstance(const ImGuiInstance&) = delete;
  ImGuiInstance(ImGuiInstance&&) = delete;
  ImGuiInstance& operator=(const ImGuiInstance&) = delete;
  ImGuiInstance& operator=(ImGuiInstance&&) = delete;
  ~ImGuiInstance();

  // Creates a new ImGuiInstance, and sets it as the current context for ImGui.
  static std::unique_ptr<ImGuiInstance> Create(CreateContract contract);

  //----------------------------------------------------------------------------
  // Initialization
  //----------------------------------------------------------------------------

  // Loads fonts initialized via ImGui::GetIO().Fonts.
  //
  // This must be called before any rendering is done, even if no custom fonts
  // were set. This may only be called *once*. A typical flow is something like
  // this:
  //
  //  bool LoadGameFonts() {
  //    ImGuiIO& io = ImGui::GetIO();
  //    io.Fonts->AddFontDefault();
  //    title_font = io.Fonts->AddFontFromFileTTF(
  //        "asset:/fonts/title_font.ttf", 100.0f);
  //    return ImGuiInstance::GetActive()->LoadFonts();
  //  }
  bool LoadFonts();

  // Adds a texture to be used with ImGui.
  //
  // The texture will remain loaded until the ImGuiInstance is destroyed.
  ImTextureID AddTexture(gb::Texture* texture);

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // Returns the current instance that ImGui is using.
  //
  // The current instance, can be switched by setting a different ImGuiContext
  // as active.
  static ImGuiInstance* GetActive() {
    return static_cast<ImGuiInstance*>(ImGui::GetIO().UserData);
  }

  // Sets this ImGuiInstance as active. All ImGui operations will happen
  // relative to this instance.
  void SetActive();

  // Returns true if this instance is active.
  bool IsActive() const { return ImGui::GetIO().UserData == this; }

  // Returns the context used to construct this instance.
  const ValidatedContext& GetContext() const { return context_; }

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  // Draws the GUI.
  //
  // This implicitly calls ImGui::Render if draw data is null (implicitly ending
  // the frame). This *must* be called within the render system's
  // BeginFrame/EndFrame calls.
  void Draw(ImDrawData* draw_data = nullptr);

 private:
  ImGuiInstance(ValidatedContext context);

  bool Init();

  ValidatedContext context_;
  std::unique_ptr<RenderScene> scene_;
  ResourceSet resources_;
  Material* material_ = nullptr;
  Mesh* mesh_ = nullptr;
  std::unique_ptr<BindingData> instance_data_;
  bool fonts_initialized_ = false;
};

}  // namespace gb

#endif  // GB_IMGUI_IMGUI_INSTANCE_H_
