// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_IMGUI_IMGUI_INSTANCE_H_
#define GB_IMGUI_IMGUI_INSTANCE_H_

#include <memory>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "gb/base/validated_context.h"
#include "gb/file/file_types.h"
#include "gb/imgui/imgui_types.h"
#include "gb/render/render_types.h"
#include "gb/resource/resource.h"
#include "gb/resource/resource_set.h"
#include "imgui.h"

namespace gb {

// This class manages an ImGuiContext, and draws its GUI with a RenderSystem.
class ImGuiInstance final {
 public:
  //----------------------------------------------------------------------------
  // Contract constraints
  //----------------------------------------------------------------------------

  // REQUIRED: FileSystem interface. The file system is used as needed by ImGui
  // (for instance, to load fonts or write configuration files).
  static GB_CONTEXT_CONSTRAINT(kConstraintFileSystem, kInRequired, FileSystem);

  // REQUIRED: RenderSystem interface. This is used to draw the GUI.
  static GB_CONTEXT_CONSTRAINT(kConstraintRenderSystem, kInRequired,
                               RenderSystem);

  // OPTIONAL: Scene order for the GUI scene. By default this is 100, which is
  // suitably late.
  static inline constexpr const char* kKeySceneOrder = "SceneOrder";
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

  // Destroying the active instance leaves no instance active.
  ~ImGuiInstance();

  // Creates a new ImGuiInstance, and makes it the active instance.
  //
  // Any number of instances may share a render system. On failure, this
  // returns null and the active instance is unchanged.
  static std::unique_ptr<ImGuiInstance> Create(CreateContract contract);

  //----------------------------------------------------------------------------
  // Initialization
  //----------------------------------------------------------------------------

  // Loads fonts initialized via ImGui::GetIO().Fonts.
  //
  // This must be called while the instance is active, before any rendering is
  // done, even if no custom fonts were set. Only the first successful call
  // loads the fonts, and later calls do nothing. A typical flow is something
  // like this:
  //
  //  bool LoadGameFonts() {
  //    ImGuiIO& io = ImGui::GetIO();
  //    io.Fonts->AddFontDefault();
  //    title_font = io.Fonts->AddFontFromFileTTF(
  //        "asset:/fonts/title_font.ttf", 100.0f);
  //    return ImGuiInstance::GetActive()->LoadFonts();
  //  }
  bool LoadFonts();

  // Adds a texture, or one texture of a texture array, to be used with ImGui.
  //
  // The texture will remain loaded until the ImGuiInstance is destroyed.
  // Adding the same texture again returns the same ID. This returns null if
  // the texture is null or the index is out of range.
  ImTextureID AddTexture(gb::Texture* texture);
  ImTextureID AddTexture(gb::TextureArray* texture_array, int index);

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // Returns the active instance that ImGui is using, or null if there is none.
  //
  // The active instance can be switched by setting a different ImGuiContext as
  // current.
  static ImGuiInstance* GetActive() {
    if (ImGui::GetCurrentContext() == nullptr) {
      return nullptr;
    }
    return static_cast<ImGuiInstance*>(ImGui::GetIO().UserData);
  }

  // Sets this ImGuiInstance as active. All ImGui operations will happen
  // relative to this instance.
  void SetActive();

  // Returns true if this instance is active.
  bool IsActive() const { return GetActive() == this; }

  // Returns the context used to construct this instance.
  const ValidatedContext& GetContext() const { return context_; }

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  // Draws the GUI.
  //
  // This implicitly calls ImGui::Render if draw data is null (implicitly ending
  // the frame). This *must* be called within the render system's
  // BeginFrame/EndFrame calls. A GUI with more than kMaxMeshVertices vertices
  // is not drawn.
  void Draw(ImDrawData* draw_data = nullptr);

 private:
  explicit ImGuiInstance(ValidatedContext context);

  bool Init();
  Material* CreateTextureMaterial();

  // Sets the font material and the mesh on the draw list, and returns the
  // texture ID that is now set.
  ImTextureID SetDrawDefaults(DrawList* draw);

  ValidatedContext context_;
  std::unique_ptr<RenderScene> scene_;
  ResourceSet resources_;
  Material* material_ = nullptr;
  Mesh* mesh_ = nullptr;
  std::unique_ptr<BindingData> instance_data_;
  bool fonts_initialized_ = false;

  // Materials from AddTexture(), keyed by the texture and texture array index
  // (-1 for a texture).
  absl::flat_hash_map<std::pair<const Resource*, int>, Material*>
      texture_materials_;
};

}  // namespace gb

#endif  // GB_IMGUI_IMGUI_INSTANCE_H_
