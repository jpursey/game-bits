// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_BINDING_H_
#define GB_RENDER_BINDING_H_

#include "gb/render/render_types.h"

namespace gb {

// A binding defines a resource that can be bound to a shader.
//
// Bindings are defined by the following:
// - Shaders: Which shader types the binding is referenced by (aka vertex and/or
//   fragment shader). A binding may be for more than one shader type.
// - Location: What binding set and binding index must be referenced in the
//   shader to access the binding. There are three binding sets (view,
//   material, and instance) which represent at what scope the bindings may be
//   tuned or changed.
// - Binding type: This determines what data or resources are associated with
//   the binding. For instance, a texture or a set of constants.
//
// Bindings are defined by the application, and must match the shaders they are
// used with. The recommended way to define a binding is via the Set* methods
// which may be chained. For example:
//     Binding()
//       .SetShaders(ShaderType::kVertex)
//       .SetLocation(BindingSet::kInstance, 0)
//       .SetConstants(model_matrix_type, DataVolatility::kPerFrame);
struct Binding {
  // Constructs a default Binding.
  //
  // This binding is invalid until the shader_types, set, index, binding_type,
  // and any additional binding type members are initialized. The recommended
  // way to do this is to call SetShaders(), SetLocation(), and one of the
  // remaining Set*() functions to initialize the binding type.
  Binding() = default;

  // Sets the shaders this binding is associated with.
  //
  // This may be used by the render backend to optimize when resources are made
  // available in the render pipeline.
  Binding& SetShaders(ShaderTypes shaders);

  // Sets the location within shaders that use this binding.
  //
  // The binding set is one of kView, kMaterial, or kInstance which determines
  // where the associated data can be modified and its scope.
  //
  // The binding index is an arbitrary index that must be within the range [0,
  // kMaxBindingIndex]. Binding indexes do *not* need to be sequential or
  // packed, but it is best to keep the indices near zero as some space may
  // still be required for unused indices less than the max index defined.
  Binding& SetLocation(BindingSet binding_set, int binding_index);

  // Sets the binding to be a 2D RGBA texture.
  //
  // The actual texture must be set within a BindingData object for this binding
  // if it is accessed by a shader.
  Binding& SetTexture();

  // Sets the binding to be a 2D RGBA texture array.
  //
  // The actual texture array must be set within a BindingData object for this
  // binding if it is accessed by a shader.
  Binding& SetTextureArray();

  // Sets the binding to be a volume texture (a sampler3D in GLSL).
  //
  // The actual volume texture must be set within a BindingData object for this
  // binding if it is accessed by a shader.
  Binding& SetVolumeTexture();

  // Sets the binding to be a constants structure.
  //
  // Constants are defined by registering a C++ type that conforms to the
  // underlying graphics API specifications with the RenderSystem via
  // RenderSystem::RegisterConstantsType.
  //
  // Volatility specifies whether the data is readable and how often it is
  // likely to be updated by the application. Choosing this correctly may have
  // an effect on both speed and space requirements. See DataVolatility for more
  // information on these.
  //
  // The actual constants in BindingData using this binding will by default be
  // all zero.
  Binding& SetConstants(
      const RenderDataType* type,
      DataVolatility data_volatility = DataVolatility::kStaticReadWrite);

  // Sets the binding to be a storage buffer of values of the type (a readonly
  // buffer block in GLSL), which is registered with
  // RenderSystem::RegisterConstantsType.
  //
  // The actual storage buffer must be set within a BindingData object for this
  // binding if it is accessed by a shader.
  Binding& SetStorageBuffer(const RenderDataType* type);

  // Returns true if the Binding is valid.
  //
  // Only valid Bindings may be used with other render types.
  bool IsValid() const;

  // Returns true if the binding is compatible.
  //
  // Compatible bindings have the same set, index, and binding type (and for
  // constants and storage buffers, the same data type TypeKey), but may have
  // different shader types or data volatility.
  bool IsCompatible(const Binding& other) const;

  // Combines the other binding into this one, if it is compatible.
  //
  // Returns false if the binding could not be combined into this binding.
  bool Combine(const Binding& other);

  // Returns true if the binding type has a data type (kConstants or
  // kStorageBuffer).
  bool HasDataType() const {
    return binding_type == BindingType::kConstants ||
           binding_type == BindingType::kStorageBuffer;
  }

  // Defines which shader the binding applies to.
  ShaderTypes shader_types;

  // The binding set determines the scope of the binding within a view.
  BindingSet set = BindingSet::kView;

  // Index for the binding within the set.
  int index = 0;

  // Defines the type of binding.
  BindingType binding_type = BindingType::kNone;

  // If the type is kConstants or kStorageBuffer, this must specify a previously
  // registered constants type: the constants, or the type of each value in the
  // storage buffer. See RenderSystem::RegisterConstantsType.
  const RenderDataType* data_type = nullptr;

  // Render data volatility determines when binding data will be changed.
  //
  // Note this only is meaningful for the binding itself. If a binding is a
  // pointer to a (potentially shared) resource like a texture, it has no
  // bearing on the volatility of the resource itself, but only to the pointer.
  // This is largely only meaningful in relation to constants which are
  // logically stored directly in the binding.
  DataVolatility volatility = DataVolatility::kStaticReadWrite;
};

inline Binding& Binding::SetShaders(ShaderTypes shaders) {
  shader_types = shaders;
  return *this;
}

inline Binding& Binding::SetLocation(BindingSet binding_set,
                                     int binding_index) {
  set = binding_set;
  index = binding_index;
  return *this;
}

inline Binding& Binding::SetTexture() {
  binding_type = BindingType::kTexture;
  data_type = nullptr;
  volatility = DataVolatility::kStaticReadWrite;
  return *this;
}

inline Binding& Binding::SetTextureArray() {
  binding_type = BindingType::kTextureArray;
  data_type = nullptr;
  volatility = DataVolatility::kStaticReadWrite;
  return *this;
}

inline Binding& Binding::SetVolumeTexture() {
  binding_type = BindingType::kVolumeTexture;
  data_type = nullptr;
  volatility = DataVolatility::kStaticReadWrite;
  return *this;
}

inline Binding& Binding::SetConstants(const RenderDataType* type,
                                      DataVolatility data_volatility) {
  binding_type = BindingType::kConstants;
  data_type = type;
  volatility = data_volatility;
  return *this;
}

inline Binding& Binding::SetStorageBuffer(const RenderDataType* type) {
  binding_type = BindingType::kStorageBuffer;
  data_type = type;
  volatility = DataVolatility::kStaticReadWrite;
  return *this;
}

inline bool Binding::IsValid() const {
  return !shader_types.IsEmpty() &&
         Union(shader_types, kAllShaderTypes) == kAllShaderTypes &&
         (binding_type == BindingType::kConstants ||
          binding_type == BindingType::kTexture ||
          binding_type == BindingType::kTextureArray ||
          binding_type == BindingType::kVolumeTexture ||
          binding_type == BindingType::kStorageBuffer) &&
         (!HasDataType() || data_type != nullptr) &&
         (volatility == DataVolatility::kPerFrame ||
          volatility == DataVolatility::kStaticReadWrite ||
          volatility == DataVolatility::kStaticWrite) &&
         (set == BindingSet::kView || set == BindingSet::kMaterial ||
          set == BindingSet::kInstance) &&
         (index >= 0 && index <= kMaxBindingIndex);
}

inline bool Binding::IsCompatible(const Binding& other) const {
  return set == other.set && index == other.index &&
         binding_type == other.binding_type &&
         (!HasDataType() || data_type == other.data_type ||
          (data_type != nullptr && other.data_type != nullptr &&
           data_type->GetType() == other.data_type->GetType()));
}

inline bool Binding::Combine(const Binding& other) {
  if (!IsCompatible(other)) {
    return false;
  }
  shader_types += other.shader_types;
  if (other.volatility > volatility) {
    volatility = other.volatility;
  }
  return true;
}

inline bool operator<(const Binding& a, const Binding& b) {
  return a.set < b.set || (a.set == b.set && a.index < b.index);
}
inline bool operator>(const Binding& a, const Binding& b) { return b < a; }
inline bool operator<=(const Binding& a, const Binding& b) { return !(b < a); }
inline bool operator>=(const Binding& a, const Binding& b) { return !(a < b); }

inline bool operator==(const Binding& a, const Binding& b) {
  return a.IsCompatible(b) && a.shader_types == b.shader_types &&
         a.volatility == b.volatility;
}
inline bool operator!=(const Binding& a, const Binding& b) { return !(a == b); }

}  // namespace gb

#endif  // GB_RENDER_BINDING_H_
