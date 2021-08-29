// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_BINDING_H_
#define GB_RENDER_BINDING_H_

#include <tuple>
#include <vector>

#include "gb/base/type_info.h"
#include "gb/render/render_assert.h"
#include "gb/render/render_types.h"

namespace gb {

// A binding defines a resource that can be bound to a shader.
//
// Bindings are defined by the following:
// - Shaders: Which shader types the binding is referenced by (aka vertex and/or
//   fragment shader). A binding may be for more than one shader type.
// - Location: What binding set and binding index must be referenced in the
//   shader to access the binding. There are three binding sets (scene,
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
  // The binding set is one of kScene, kMaterial, or kInstance which determines
  // where the associated data can be modified and it scope.
  //
  // The binding index is an arbtrary index that must be within the range [0,
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

  // Sets the binding to be a constants structure/
  //
  // Constants are defined by registering a C++ type that conforms to the
  // underlying graphics API specifications with the RenderSystem via
  // RenderSystem::RegisterConstantsType.
  //
  // Volatility specifies whether the data is readable and how often it is
  // likely to be updated by the application. Choosing this correctly may has an
  // effect on both speed and space requirements. See DataVolatility for more
  // information on these.
  //
  // The actual constants in BindingData using this binding will by default be
  // all zero.
  Binding& SetConstants(
      const RenderDataType* type,
      DataVolatility data_volatility = DataVolatility::kStaticReadWrite);

  // Returns true if the Binding is valid.
  //
  // Only valid Bindings may be used with other render types.
  bool IsValid() const;

  // Returns true if the binding is compatible.
  //
  // Compatible bindings have the same set, index, and binding type, but may
  // have different shader types or data volatility.
  bool IsCompatible(const Binding& other) const;

  // Combines the other binding into this one, if it is compatible.
  //
  // Returns false if the binding could not be combined into this binding.
  bool Combine(const Binding& other);

  // Defines which shader the binding applies to.
  ShaderTypes shader_types;

  // The binding set determines the scope of the binding within a scene.
  BindingSet set = BindingSet::kScene;

  // Index for the binding within the set.
  int index = 0;

  // Defines the type of binding.
  //
  // Each binding type may have
  BindingType binding_type = BindingType::kNone;

  // If the type is kConstants, this must specify a previously registered
  // constants type. See RenderSystem::RegisterConstantsType.
  const RenderDataType* constants_type = nullptr;

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
  constants_type = nullptr;
  volatility = DataVolatility::kStaticReadWrite;
  return *this;
}

inline Binding& Binding::SetTextureArray() {
  binding_type = BindingType::kTextureArray;
  constants_type = nullptr;
  volatility = DataVolatility::kStaticReadWrite;
  return *this;
}

inline Binding& Binding::SetConstants(const RenderDataType* type,
                                      DataVolatility data_volatility) {
  binding_type = BindingType::kConstants;
  constants_type = type;
  volatility = data_volatility;
  return *this;
}

inline bool Binding::IsValid() const {
  return !shader_types.IsEmpty() &&
         Union(shader_types, kAllShaderTypes) == kAllShaderTypes &&
         (binding_type == BindingType::kConstants ||
          binding_type == BindingType::kTexture ||
          binding_type == BindingType::kTextureArray) &&
         (binding_type != BindingType::kConstants ||
          constants_type != nullptr) &&
         (volatility == DataVolatility::kPerFrame ||
          volatility == DataVolatility::kStaticReadWrite ||
          volatility == DataVolatility::kStaticWrite) &&
         (set == BindingSet::kScene || set == BindingSet::kMaterial ||
          set == BindingSet::kInstance) &&
         (index >= 0 && index < kMaxBindingIndex);
}

inline bool Binding::IsCompatible(const Binding& other) const {
  return set == other.set && index == other.index &&
         binding_type == other.binding_type &&
         (binding_type != BindingType::kConstants ||
          constants_type == other.constants_type);
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
  return a.shader_types == b.shader_types && a.binding_type == b.binding_type &&
         (a.binding_type != BindingType::kConstants ||
          a.constants_type == b.constants_type ||
          (a.constants_type != nullptr && b.constants_type != nullptr &&
           a.constants_type->GetType() == b.constants_type->GetType())) &&
         a.volatility == b.volatility && a.set == b.set && a.index == b.index;
}
inline bool operator!=(const Binding& a, const Binding& b) { return !(a == b); }

}  // namespace gb

#endif  // GB_RENDER_BINDING_H_
