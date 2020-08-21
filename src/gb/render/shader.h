// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_SHADER_H_
#define GB_RENDER_SHADER_H_

#include <memory>
#include <vector>

#include "absl/types/span.h"
#include "gb/render/binding.h"
#include "gb/render/render_types.h"
#include "gb/render/shader_code.h"
#include "gb/resource/resource.h"

namespace gb {

// A shader defines a programmable portion of a render pipeline, and is used to
// define material types.
//
// This class is thread-compatible.
class Shader final : public Resource {
 public:
  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // Returns the shader type.
  ShaderType GetType() const { return type_; }

  // Returns the underlying shader code.
  ShaderCode* GetCode() const { return code_.get(); }

  // Returns the inputs / outputs for this shader.
  absl::Span<const ShaderParam> GetInputs() const { return inputs_; }
  absl::Span<const ShaderParam> GetOutputs() const { return outputs_; }

  // Returns the bindings for this shader.
  absl::Span<const Binding> GetBindings() const { return bindings_; }

  //----------------------------------------------------------------------------
  // Internal
  //----------------------------------------------------------------------------

  Shader(RenderInternal, ResourceEntry entry, ShaderType type,
         std::unique_ptr<ShaderCode> code, absl::Span<const Binding> bindings,
         absl::Span<const ShaderParam> inputs,
         absl::Span<const ShaderParam> outputs)
      : Resource(std::move(entry)),
        type_(type),
        code_(std::move(code)),
        bindings_(bindings.begin(), bindings.end()),
        inputs_(inputs.begin(), inputs.end()),
        outputs_(outputs.begin(), outputs.end()) {}

 protected:
  ~Shader() override = default;

  const ShaderType type_;
  const std::unique_ptr<ShaderCode> code_;
  const std::vector<Binding> bindings_;
  const std::vector<ShaderParam> inputs_;
  const std::vector<ShaderParam> outputs_;
};

}  // namespace gb

#endif  // GB_RENDER_SHADER_H_
