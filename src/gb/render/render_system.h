// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_RENDER_SYSTEM_H_
#define GB_RENDER_RENDER_SYSTEM_H_

#include <memory>
#include <string>
#include <string_view>
#include <tuple>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "flatbuffers/flatbuffers.h"
#include "gb/base/validated_context.h"
#include "gb/file/file_types.h"
#include "gb/render/binding.h"
#include "gb/render/draw_list.h"
#include "gb/render/material.h"
#include "gb/render/material_config.h"
#include "gb/render/material_type.h"
#include "gb/render/mesh.h"
#include "gb/render/render_scene.h"
#include "gb/render/render_scene_type.h"
#include "gb/render/render_types.h"
#include "gb/render/sampler_options.h"
#include "gb/render/shader.h"
#include "gb/render/texture.h"
#include "gb/resource/resource_ptr.h"
#include "gb/resource/resource_set.h"
#include "gb/resource/resource_types.h"

namespace gb {

// This class defines an API-independent rendering interface that supports
// hardware acceleration.
//
// This is a relatively low level interface that aims to provide a graphics API
// independent abstraction for rendering and management of rendering resources.
// This does not provide higher level functionality like cameras, lighting,
// scene culling, etc. It also does not define prebuilt shaders or graphic
// algorithms for doing any particular sort of rendering. These are left to
// higher level scene libraries to build on top of the core rendering
// interfaces.
//
// API specific implementations (including any additional lower level
// optimizations and features) are defined by implementing the RenderBackend
// interface which the RenderSystem uses. A valid RenderBackend is required in
// order to construct the RenderSystem.
//
// See section details for thread-safety.
class RenderSystem final {
 public:
  //----------------------------------------------------------------------------
  // Contract constraints
  //----------------------------------------------------------------------------

  // REQUIRED: Backend implementation.
  static GB_CONTEXT_CONSTRAINT(kConstraintBackend, kInRequired, RenderBackend);

  // REQUIRED: ResourceSystem interface. The RenderSystem manages all render
  // resources, and requires a resource system to hook into.
  static GB_CONTEXT_CONSTRAINT(kConstraintResourceSystem, kInRequired,
                               ResourceSystem);

  // REQUIRED: FileSystem interface. The file system is used to load/save render
  // resources as requested through the resource system.
  static GB_CONTEXT_CONSTRAINT(kConstraintFileSystem, kInRequired, FileSystem);

  // OPTIONAL: Debug flag which controls whether validation and debug logging
  // are enabled.
  static inline constexpr char* kKeyEnableDebug = "EnableDebug";
#ifdef _DEBUG
  static inline constexpr bool kDefaultEnableDebug = true;
#else
  static inline constexpr bool kDefaultEnableDebug = false;
#endif
  static GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(kConstraintEnableDebug,
                                             kInOptional, bool, kKeyEnableDebug,
                                             kDefaultEnableDebug);

  // OPTIONAL: Editor flag which ensures that all resources are by default
  // editable when loaded via the resource system (at least
  // DataVolatility::kStaticReadWrite). This also the only mode which allows
  // saving Shader resources (which otherwise are always write-only).
  static inline constexpr char* kKeyEnableEdit = "EnableEdit";
  static GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(kConstraintEnableEdit, kInOptional,
                                             bool, kKeyEnableEdit, false);

  // Contract for render system creation.
  using Contract =
      ContextContract<kConstraintBackend, kConstraintFileSystem,
                      kConstraintResourceSystem, kConstraintEnableDebug,
                      kConstraintEnableEdit>;

  // OPTIONAL: Specifies sampler options for the texture load contract. If the
  // sampler options are not specified, then either the sampler options in the
  // resource file will be used, or the default (if there are no sampler options
  // in the texture file -- for instance a regular image file).
  static GB_CONTEXT_CONSTRAINT(kConstraintSamplerOptions, kInOptional,
                               SamplerOptions);

  // Contract for load context of texture resources.
  using TextureLoadContract = ContextContract<kConstraintSamplerOptions>;

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // Creates a new render system given the provided contract.
  //
  // Returns null if the contract is invalid, or the render system could not
  // successfully be initialized.
  static std::unique_ptr<RenderSystem> Create(Contract contract);

  RenderSystem(const RenderSystem&) = delete;
  RenderSystem(RenderSystem&&) = delete;
  RenderSystem& operator=(const RenderSystem&) = delete;
  RenderSystem& operator=(RenderSystem&&) = delete;
  ~RenderSystem();

  //----------------------------------------------------------------------------
  // Initialization
  //
  // Thread-safety: Initialization functions are thread-compatible relative to
  // all other other methods. The safest approach is to do all initialization
  // first, before making any other calls (in any thread).
  //----------------------------------------------------------------------------

  // Registers a constants type with a name, to allow serialization of material
  // types and provide type safety when setting constant data. This must be a
  // trivially copyable type that conforms to the layout specification of the
  // underlying graphics API (stb140 for OpenGL or Vulkan).
  template <typename Type>
  const RenderDataType* RegisterConstantsType(std::string_view name);

  // Registers a vertex type with a name, to allow serialization of material
  // types and provide type safety when setting constant data. This must be a
  // trivially copyable type that is packed (each shader value has 4-byte
  // alignment).
  template <typename Type>
  const VertexType* RegisterVertexType(
      std::string_view name, absl::Span<const ShaderValue> attributes);

  // Registers a new RenderSceneType with a name, to allow serialization of
  // material types.
  //
  // A RenderSceneType is required in order to create RenderScene instances and
  // do any rendering.
  //
  // After registering a scene type, any binding data defaults should be set
  // before calling CreateScene or CreateMaterialType.
  RenderSceneType* RegisterSceneType(std::string_view name,
                                     absl::Span<const Binding> bindings);

  //----------------------------------------------------------------------------
  // Properies
  //
  // Thread-safety: All property methods are thread-safe (except relative to
  // initialization functions).
  //----------------------------------------------------------------------------

  // Retrieves the context used by the render system.
  const ValidatedContext& GetContext() const { return context_; }

  // Returns the requested constants type if it exists or null otherwise.
  const RenderDataType* GetConstantsType(std::string_view name) const;

  // Returns the requested vertex type if it exists or null otherwise.
  const VertexType* GetVertexType(std::string_view name) const;

  // Returns the requested render scene type if it exists or null otherwise.
  RenderSceneType* GetSceneType(std::string_view name) const;

  // Return frame dimensions in pixels or null otherwise.
  FrameDimensions GetFrameDimensions();

  //----------------------------------------------------------------------------
  // Resource / Scene creation
  //
  // Thread-safety: All resource creation, saves, and loads (generally via the
  // ResourceSystem) are thread-safe (except relative to initialization
  // functions). However, all functions are thread-compatible relative to
  // modification of the resources referenced by a call.
  //----------------------------------------------------------------------------

  // Creates a render scene which must be used in order to draw any render
  // resources.
  //
  // The scene order is used to define a global processing order across scenes.
  // See RenderScene for more details on how this is used.
  std::unique_ptr<RenderScene> CreateScene(RenderSceneType* scene_type,
                                           int scene_order = 0);

  // Creates a mesh with the specified parameters.
  //
  // If successful, the mesh will be created empty. The caller should call
  // Set or Edit to initialize its contents.
  ResourcePtr<Mesh> CreateMesh(Material* material, DataVolatility volatility,
                               int max_vertices, int max_triangles);
  Mesh* CreateMesh(ResourceSet* resource_set, Material* material,
                   DataVolatility volatility, int max_vertices,
                   int max_triangles);

  // Saves a mesh to the specified resource name.
  //
  // The mesh must be readable (not DataVolatility::kStaticWrite), and all
  // dependent resources must already be saved. Optionally, DataVolatility may
  // be specified for the mesh to indicate what it should be when it is reloaded
  // later.
  //
  // Returns true if the mesh was successfully saved.
  bool SaveMesh(std::string_view name, Mesh* mesh,
                DataVolatility volatility = DataVolatility::kStaticWrite);

  // Creates a material from a material type.
  //
  // The created material will use the default material and instance binding
  // data from the material type. These may be overridden after the material is
  // created.
  ResourcePtr<Material> CreateMaterial(MaterialType* material_type);
  Material* CreateMaterial(ResourceSet* resource_set,
                           MaterialType* material_type);

  // Saves a material to the specified resource name.
  //
  // All data bindings and must be readable (not DataVolatility::kStaticWrite),
  // and all dependent resources must already be saved.
  //
  // Returns true if the material was successfully saved.
  bool SaveMaterial(std::string_view name, Material* material);

  // Creates a new material type from the specified shaders.
  //
  // The shaders must be compatible with each other (vertex outputs match
  // fragment inputs) and with the RenderSceneType bindings (must not have any
  // conflicting bindings). Bindings are not conflicting if they have the same
  // type for the same set and binding index. It is ok if the data volatility
  // differs. The resulting material type bindings will be the union of the
  // shader and scene bindings, using the most permissive data volatility if
  // there is a difference.
  //
  // Default material and instance binding data will be default (all zero or
  // null), and the caller should initialize this before using the material
  // type.
  ResourcePtr<MaterialType> CreateMaterialType(
      RenderSceneType* scene_type, const VertexType* vertex_type,
      Shader* vertex_shader, Shader* fragment_shader,
      const MaterialConfig& config = {});
  MaterialType* CreateMaterialType(ResourceSet* resource_set,
                                   RenderSceneType* scene_type,
                                   const VertexType* vertex_type,
                                   Shader* vertex_shader,
                                   Shader* fragment_shader,
                                   const MaterialConfig& config = {});

  // Saves a material type to the specified resource name.
  //
  // All data bindings and must be readable (not DataVolatility::kStaticWrite),
  // and all dependent resources must already be saved.
  //
  // Returns true if the material type was successfully saved.
  bool SaveMaterialType(std::string_view name, MaterialType* material_type);

  // Creates a shader from the specified bindings and API specific shader code
  //
  // Common bindings defined by a RenderSceneType or paired Shader are
  // implicitly added to the provided bindings when creating a MaterialType, and
  // must either match or not be specified in bindings parameters. It is
  // recommended to only specify the bindings that the shader code actually
  // uses. The inputs, outputs, and bindings MUST match the underyling shader
  // code, or the results will be undefined (crash or corruption is likely).
  ResourcePtr<Shader> CreateShader(ShaderType type,
                                   std::unique_ptr<ShaderCode> shader_code,
                                   absl::Span<const Binding> bindings,
                                   absl::Span<const ShaderParam> inputs,
                                   absl::Span<const ShaderParam> outputs);
  Shader* CreateShader(ResourceSet* resource_set, ShaderType type,
                       std::unique_ptr<ShaderCode> shader_code,
                       absl::Span<const Binding> bindings,
                       absl::Span<const ShaderParam> inputs,
                       absl::Span<const ShaderParam> outputs);

  // Saves a shader to the specified resource name.
  //
  // Edit mode must be enabled (see kConstraintEnableEdit).
  //
  // Returns true if the material type was successfully saved.
  bool SaveShader(std::string_view name, Shader* shader);

  // Creates or backend specific shader code to be used with a shader.
  //
  // The shader code specified must be in a form expected by the render backend
  // used by the render system. For instance, for the Vulkan backend this must
  // be SPIR-V code.
  //
  // Returns null if the shader code could not be created.
  std::unique_ptr<ShaderCode> CreateShaderCode(const void* code,
                                               int64_t code_size);
  std::unique_ptr<ShaderCode> LoadShaderCode(std::string_view filename);

  // Creates a texture with the specified parameters.
  //
  // If successful, the texture will be created uninitialized (pixel values are
  // undefined). The caller should call Set or Edit to initialize its contents.
  ResourcePtr<Texture> CreateTexture(
      DataVolatility volatility, int width, int height,
      const SamplerOptions& options = SamplerOptions());
  Texture* CreateTexture(ResourceSet* resource_set, DataVolatility volatility,
                         int width, int height,
                         const SamplerOptions& options = SamplerOptions());

  // Saves a texture to the specified resource name.
  //
  // The texture must be readable (not DataVolatility::kStaticWrite).
  // Optionally, DataVolatility may be specified for the mesh to indicate what
  // it should be when it is reloaded later.
  //
  // Returns true if the texture was successfully saved.
  bool SaveTexture(std::string_view name, Texture* texture,
                   DataVolatility volatility = DataVolatility::kStaticWrite);

  //----------------------------------------------------------------------------
  // Rendering
  //
  // Thread-safety: These functions are thread-compatible relative to each
  // other. After a resource is queued for rendering, it CANNOT be modified
  // until the subsequent EndFrame is called. Further, if resources are modified
  // on a different thread than they are rendered, then some form of external
  // synchronization is required.
  //
  // Note: It is allowed to build DrawLists in simultaneously across multiple
  // threads. Only calls to Draw must happen synchronized relative to each
  // other.
  //----------------------------------------------------------------------------

  // Sets the clear color for the background before rendering takes place.
  void SetClearColor(Pixel color);

  // Begins rendering, returning a render context for the current frame.
  //
  // This will block until a frame is ready to be rendered. If rendering is
  // not currently possible (for instance the window has zero size, or is
  // minimized), this will return false and the game should try again later.
  bool BeginFrame();

  // Render a mesh in the scene using the specified instance data.
  //
  // This method does not necessarily preserve order relative to other draw
  // methods. The render backend is free to reorder draw calls to maximize
  // performance. If explicit ordering of relative draw calls is required, use
  // a DrawList instead. To force global ordering, use separate scenes with a
  // defined order; however this should be reserved for a small number of scenes
  // as it is a significantly more heavyweight approach.
  //
  // The scene, mesh, and instance_data must all be compatible with each other.
  // This is based on the mesh's material's material type:
  // - The scene's type must match the material type's scene type.
  // - The instance data be created from a material of the same material type.
  void Draw(RenderScene* scene, Mesh* mesh, BindingData* instance_data);

  // Renderss a list of ordered draw commands in the scene.
  //
  // This method does not necessarily preserve order relative to other draw
  // methods. The render backend is free to reorder draw calls to maximize
  // performance. To force global ordering, use separate scenes with a defined
  // order; however this should be reserved for a small number of scenes as it
  // is a significantly more heavyweight approach.
  //
  // The scene must be compatible with all mesh, and instance_data must in the
  // DrawList. The scene's type must match each material type's scene type.
  void Draw(RenderScene* scene, const DrawList& commands);

  // End frame will transfer all registered commands to the graphics card and
  // present the frame.
  void EndFrame();

 private:
  RenderSystem(ValidatedContext context);

  bool Init();

  Mesh* DoCreateMesh(Material* material, DataVolatility volatility,
                     int max_vertices, int max_triangles);
  Mesh* LoadMeshChunk(Context* context, const fbs::MeshChunk* chunk,
                      ResourceEntry entry);
  bool SaveMeshChunk(Context* context, Mesh* mesh,
                     flatbuffers::FlatBufferBuilder* builder);

  Material* DoCreateMaterial(MaterialType* material_type);
  Material* LoadMaterialChunk(Context* context, const fbs::MaterialChunk* chunk,
                              ResourceEntry entry);
  bool SaveMaterialChunk(Context* context, Material* material,
                         flatbuffers::FlatBufferBuilder* builder);

  bool ValidateMaterialTypeArguments(RenderSceneType* scene_type,
                                     const VertexType* vertex_type,
                                     Shader* vertex_shader,
                                     Shader* fragment_shader);
  MaterialType* DoCreateMaterialType(RenderSceneType* scene_type,
                                     const VertexType* vertex_type,
                                     Shader* vertex_shader,
                                     Shader* fragment_shader,
                                     const MaterialConfig& config);
  MaterialType* LoadMaterialTypeChunk(Context* context,
                                      const fbs::MaterialTypeChunk* chunk,
                                      ResourceEntry entry);
  bool SaveMaterialTypeChunk(Context* context, MaterialType* material_type,
                             flatbuffers::FlatBufferBuilder* builder);

  Shader* DoCreateShader(ShaderType type,
                         std::unique_ptr<ShaderCode> shader_code,
                         absl::Span<const Binding> bindings,
                         absl::Span<const ShaderParam> inputs,
                         absl::Span<const ShaderParam> outputs);
  Shader* LoadShaderChunk(Context* context, const fbs::ShaderChunk* chunk,
                          ResourceEntry entry);
  bool SaveShaderChunk(Context* context, Shader* shader,
                       flatbuffers::FlatBufferBuilder* builder);

  Texture* DoCreateTexture(DataVolatility volatility, int width, int height,
                           const SamplerOptions& options);
  Texture* LoadTexture(Context* context, std::string_view name);
  Texture* LoadTextureChunk(Context* context, const fbs::TextureChunk* chunk,
                            ResourceEntry entry);
  bool SaveTextureChunk(Context* context, Texture* texture,
                        flatbuffers::FlatBufferBuilder* builder);
  Texture* LoadStbTexture(TextureLoadContract contract, File* file);

  const RenderDataType* DoRegisterConstantsType(std::string_view name,
                                                TypeKey* type, size_t size);
  const VertexType* DoRegisterVertexType(
      std::string_view name, TypeKey* type, size_t size,
      absl::Span<const ShaderValue> attributes);

  // Members set at creation and never modified afterward. These do not require
  // additional synchronization.
  ValidatedContext context_;
  RenderBackend* backend_;
  std::unique_ptr<ResourceManager> resource_manager_;
  bool debug_ = false;
  bool edit_ = false;
  std::unique_ptr<ResourceFileReader> resource_reader_;
  std::unique_ptr<ResourceFileWriter> resource_writer_;

  // Members set by initialization functions only. These must be externally
  // synchronized.
  absl::flat_hash_map<std::string, std::unique_ptr<RenderDataType>>
      constants_types_;
  absl::flat_hash_map<std::string, std::unique_ptr<VertexType>> vertex_types_;
  absl::flat_hash_map<std::string, std::unique_ptr<RenderSceneType>>
      scene_types_;

  // Members set during rendering. All rendering is externally synchronized.
  bool is_rendering_ = false;
};

template <typename Type>
inline const RenderDataType* RenderSystem::RegisterConstantsType(
    std::string_view name) {
  static_assert(std::is_trivially_copyable_v<Type>,
                "Shader constants types must be trivially copyable");
  return DoRegisterConstantsType(name, TypeKey::Get<Type>(), sizeof(Type));
}

template <typename Type>
inline const VertexType* RenderSystem::RegisterVertexType(
    std::string_view name, absl::Span<const ShaderValue> attributes) {
  static_assert(std::is_trivially_copyable_v<Type>,
                "Vertex types must be trivially copyable");
  return DoRegisterVertexType(name, TypeKey::Get<Type>(), sizeof(Type),
                              attributes);
}

inline ResourcePtr<Mesh> RenderSystem::CreateMesh(Material* material,
                                                  DataVolatility volatility,
                                                  int max_vertices,
                                                  int max_triangles) {
  return DoCreateMesh(material, volatility, max_vertices, max_triangles);
}

inline Mesh* RenderSystem::CreateMesh(ResourceSet* resource_set,
                                      Material* material,
                                      DataVolatility volatility,
                                      int max_vertices, int max_triangles) {
  Mesh* mesh = DoCreateMesh(material, volatility, max_vertices, max_triangles);
  resource_set->Add(mesh);
  return mesh;
}

inline ResourcePtr<Material> RenderSystem::CreateMaterial(
    MaterialType* material_type) {
  return DoCreateMaterial(material_type);
}

inline Material* RenderSystem::CreateMaterial(ResourceSet* resource_set,
                                              MaterialType* material_type) {
  Material* material = DoCreateMaterial(material_type);
  resource_set->Add(material);
  return material;
}

inline ResourcePtr<MaterialType> RenderSystem::CreateMaterialType(
    RenderSceneType* scene_type, const VertexType* vertex_type,
    Shader* vertex_shader, Shader* fragment_shader,
    const MaterialConfig& config) {
  return DoCreateMaterialType(scene_type, vertex_type, vertex_shader,
                              fragment_shader, config);
}

inline MaterialType* RenderSystem::CreateMaterialType(
    ResourceSet* resource_set, RenderSceneType* scene_type,
    const VertexType* vertex_type, Shader* vertex_shader,
    Shader* fragment_shader, const MaterialConfig& config) {
  MaterialType* material_type = DoCreateMaterialType(
      scene_type, vertex_type, vertex_shader, fragment_shader, config);
  resource_set->Add(material_type);
  return material_type;
}

inline ResourcePtr<Shader> RenderSystem::CreateShader(
    ShaderType type, std::unique_ptr<ShaderCode> shader_code,
    absl::Span<const Binding> bindings, absl::Span<const ShaderParam> inputs,
    absl::Span<const ShaderParam> outputs) {
  return DoCreateShader(type, std::move(shader_code), bindings, inputs,
                        outputs);
}

inline Shader* RenderSystem::CreateShader(
    ResourceSet* resource_set, ShaderType type,
    std::unique_ptr<ShaderCode> shader_code, absl::Span<const Binding> bindings,
    absl::Span<const ShaderParam> inputs,
    absl::Span<const ShaderParam> outputs) {
  Shader* shader =
      DoCreateShader(type, std::move(shader_code), bindings, inputs, outputs);
  resource_set->Add(shader);
  return shader;
}

inline ResourcePtr<Texture> RenderSystem::CreateTexture(
    DataVolatility volatility, int width, int height,
    const SamplerOptions& options) {
  return DoCreateTexture(volatility, width, height, options);
}

inline Texture* RenderSystem::CreateTexture(ResourceSet* resource_set,
                                            DataVolatility volatility,
                                            int width, int height,
                                            const SamplerOptions& options) {
  Texture* texture = DoCreateTexture(volatility, width, height, options);
  resource_set->Add(texture);
  return texture;
}

}  // namespace gb

#endif  // GB_RENDER_RENDER_SYSTEM_H_
