// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_BINDING_DATA_H_
#define GB_RENDER_BINDING_DATA_H_

#include "gb/render/binding.h"
#include "gb/render/render_types.h"
#include "gb/render/texture.h"
#include "gb/render/texture_array.h"

namespace gb {

// This class contains all the binding data for a specific binding set.
//
// Binding data defines the actual resources that are accessible from shaders of
// a specific MaterialType. Separate BindingData is associated at the scene,
// material, and instance scopes. Binding data may be retrieved or created by
// calling the appropriate function on RenderSceneType, MaterialType, and
// Material.
//
// BindingData may be changed at any time (whether rendering a frame or not),
// but is only applied when RenderSystem::EndFrame is called. As such, it is not
// meaningful to change BindingData between multiple Draw calls that use it, as
// only the final modifications will apply to *all* the Draw calls.
//
// This class and all derived classes must be thread-compatible.
class BindingData {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  BindingData(const BindingData&) = delete;
  BindingData(BindingData&&) = delete;
  BindingData& operator=(const BindingData&) = delete;
  BindingData& operator=(BindingData&&) = delete;
  virtual ~BindingData() = default;

  //----------------------------------------------------------------------------
  // Properies
  //----------------------------------------------------------------------------

  // Returns the binding set this binding data is for.
  BindingSet GetSet() const { return set_; }

  // Helper validation methods to determine if a binding index has the expected
  // type.
  template <typename Type>
  bool IsConstants(int index) {
    return Validate(index, TypeKey::Get<Type>());
  }
  bool IsTexture(int index) {
    return Validate(index, TypeKey::Get<Texture*>());
  }

  // Get or set constant data.
  //
  // Calling this on an undefined binding index for the set, or a binding of a
  // different binding type or constants type is undefined behavior, and likely
  // will result in a crash.
  //
  // Note that GetConstants will not return anything if the binding's data
  // volatility was kStaticWrite.
  template <typename Type>
  void SetConstants(int index, const Type& constants);
  template <typename Type>
  void GetConstants(int index, Type* constants) const;

  // Get or set texture resource.
  //
  // Calling this on an undefined binding index for the set, or a binding of a
  // different binding type is undefined behavior, and likely will result in a
  // crash.
  void SetTexture(int index, Texture* texture);
  const Texture* GetTexture(int index) const;

  // Get or set texture array resource.
  //
  // Calling this on an undefined binding index for the set, or a binding of a
  // different binding type is undefined behavior, and likely will result in a
  // crash.
  void SetTextureArray(int index, TextureArray* texture_array);
  const TextureArray* GetTextureArray(int index) const;

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  // Adds any resource dependencies in this binding data to "dependencies".
  void GetDependencies(ResourceDependencyList* dependencies) const;

  //----------------------------------------------------------------------------
  // Internal
  //----------------------------------------------------------------------------

  RenderPipeline* GetPipeline(RenderInternal) const { return pipeline_; }
  void SetInternal(RenderInternal, int index, TypeKey* type, const void* data);
  void GetInternal(RenderInternal, int index, TypeKey* type, void* data) const;

 protected:
  BindingData(RenderPipeline* pipeline, BindingSet set)
      : pipeline_(pipeline), set_(set) {}

  //----------------------------------------------------------------------------
  // Derived class interface
  //----------------------------------------------------------------------------

  // Called when render asserts are enabled, this should validate the
  // parameters.
  virtual bool Validate(int index, TypeKey* type) const = 0;

  // Implementation functions for all Set/Get* functions. The derived class
  // can assume parameters are validated and of the right type as follows:
  // - BindingType::kConstants: "value" points to the actual constants type.
  // - BindingType::kTexture: "value" points to a Texture* (aka it is actually a
  //   Texture**).
  virtual void DoSet(int index, const void* value) = 0;
  virtual void DoGet(int index, void* value) const = 0;

  // Implementation for GetDependencies.
  //
  // This should add any resources that are initialized to dependencies (for
  // instance, any defined texture resources).
  virtual void DoGetDependencies(
      ResourceDependencyList* dependencies) const = 0;

 private:
  RenderPipeline* const pipeline_;
  const BindingSet set_;
};

inline void BindingData::GetDependencies(
    ResourceDependencyList* dependencies) const {
  DoGetDependencies(dependencies);
}

inline void BindingData::SetTexture(int index, Texture* texture) {
  RENDER_ASSERT(Validate(index, TypeKey::Get<Texture*>()));
  DoSet(index, &texture);
}

inline const Texture* BindingData::GetTexture(int index) const {
  RENDER_ASSERT(Validate(index, TypeKey::Get<Texture*>()));
  Texture* texture = nullptr;
  DoGet(index, &texture);
  return texture;
}

inline void BindingData::SetTextureArray(int index, TextureArray* texture) {
  RENDER_ASSERT(Validate(index, TypeKey::Get<TextureArray*>()));
  DoSet(index, &texture);
}

inline const TextureArray* BindingData::GetTextureArray(int index) const {
  RENDER_ASSERT(Validate(index, TypeKey::Get<TextureArray*>()));
  TextureArray* texture_array = nullptr;
  DoGet(index, &texture_array);
  return texture_array;
}

template <typename Type>
inline void BindingData::SetConstants(int index, const Type& constants) {
  RENDER_ASSERT(Validate(index, TypeKey::Get<Type>()));
  DoSet(index, &constants);
}

template <typename Type>
inline void BindingData::GetConstants(int index, Type* constants) const {
  RENDER_ASSERT(Validate(index, TypeKey::Get<Type>()));
  DoGet(index, constants);
}

inline void BindingData::SetInternal(RenderInternal, int index, TypeKey* type,
                                     const void* data) {
  RENDER_ASSERT(Validate(index, type));
  DoSet(index, data);
}

inline void BindingData::GetInternal(RenderInternal, int index, TypeKey* type,
                                     void* data) const {
  RENDER_ASSERT(Validate(index, type));
  DoGet(index, data);
}

}  // namespace gb

#endif  // GB_RENDER_BINDING_DATA_H_
