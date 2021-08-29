// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_RENDER_RESOURCE_CHUNKS_H_
#define GB_RENDER_RENDER_RESOURCE_CHUNKS_H_

#include <type_traits>

#include "gb/file/chunk_types.h"
#include "gb/render/material_config.h"
#include "gb/render/render_resource_generated.h"
#include "gb/render/render_types.h"
#include "gb/render/sampler_options.h"

namespace gb {

// Resource chunk types
inline constexpr ChunkType kChunkTypeMaterial = {'G', 'B', 'M', 'A'};
inline constexpr ChunkType kChunkTypeMaterialType = {'G', 'B', 'M', 'T'};
inline constexpr ChunkType kChunkTypeMesh = {'G', 'B', 'M', 'E'};
inline constexpr ChunkType kChunkTypeShader = {'G', 'B', 'S', 'H'};
inline constexpr ChunkType kChunkTypeTexture = {'G', 'B', 'T', 'X'};
inline constexpr ChunkType kChunkTypeTextureArray = {'G', 'B', 'T', 'A'};

template <typename FbsType, typename GbType>
inline FbsType RenderEnumToFbs(GbType type) {
  return static_cast<FbsType>(static_cast<std::underlying_type_t<FbsType>>(
      static_cast<std::underlying_type_t<GbType>>(type)));
}
template <typename FbsType, typename GbType>
inline GbType RenderEnumFromFbs(FbsType type) {
  return static_cast<GbType>(static_cast<std::underlying_type_t<GbType>>(
      static_cast<std::underlying_type_t<FbsType>>(type)));
}

inline fbs::ShaderType ToFbs(gb::ShaderType type) {
  return RenderEnumToFbs<fbs::ShaderType, gb::ShaderType>(type);
}
inline gb::ShaderType FromFbs(fbs::ShaderType type) {
  return RenderEnumFromFbs<fbs::ShaderType, gb::ShaderType>(type);
}

inline fbs::BindingSet ToFbs(gb::BindingSet type) {
  return RenderEnumToFbs<fbs::BindingSet, gb::BindingSet>(type);
}
inline gb::BindingSet FromFbs(fbs::BindingSet type) {
  return RenderEnumFromFbs<fbs::BindingSet, gb::BindingSet>(type);
}

inline fbs::BindingType ToFbs(gb::BindingType type) {
  return RenderEnumToFbs<fbs::BindingType, gb::BindingType>(type);
}
inline gb::BindingType FromFbs(fbs::BindingType type) {
  return RenderEnumFromFbs<fbs::BindingType, gb::BindingType>(type);
}

inline fbs::DataVolatility ToFbs(gb::DataVolatility type) {
  return RenderEnumToFbs<fbs::DataVolatility, gb::DataVolatility>(type);
}
inline gb::DataVolatility FromFbs(fbs::DataVolatility type) {
  return RenderEnumFromFbs<fbs::DataVolatility, gb::DataVolatility>(type);
}

inline fbs::DepthMode ToFbs(gb::DepthMode type) {
  return RenderEnumToFbs<fbs::DepthMode, gb::DepthMode>(type);
}
inline gb::DepthMode FromFbs(fbs::DepthMode type) {
  return RenderEnumFromFbs<fbs::DepthMode, gb::DepthMode>(type);
}

inline fbs::CullMode ToFbs(gb::CullMode type) {
  return RenderEnumToFbs<fbs::CullMode, gb::CullMode>(type);
}
inline gb::CullMode FromFbs(fbs::CullMode type) {
  return RenderEnumFromFbs<fbs::CullMode, gb::CullMode>(type);
}

inline fbs::SamplerAddressMode ToFbs(gb::SamplerAddressMode type) {
  return RenderEnumToFbs<fbs::SamplerAddressMode, gb::SamplerAddressMode>(type);
}
inline gb::SamplerAddressMode FromFbs(fbs::SamplerAddressMode type) {
  return RenderEnumFromFbs<fbs::SamplerAddressMode, gb::SamplerAddressMode>(
      type);
}

inline fbs::ShaderValue ToFbs(gb::ShaderValue type) {
  return RenderEnumToFbs<fbs::ShaderValue, gb::ShaderValue>(type);
}

}  // namespace gb

#endif  // GB_RENDER_RENDER_RESOURCE_CHUNKS_H_
