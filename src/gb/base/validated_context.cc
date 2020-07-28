#include "gb/base/validated_context.h"

#include <sstream>

#include "absl/strings/str_cat.h"
#include "glog/logging.h"

namespace gb {

namespace {

std::string ToString(std::string_view type_name, std::string_view name) {
  if (type_name.empty()) {
    type_name = "unspecified-type";
  }
  if (name.empty()) {
    return std::string(type_name.data(), type_name.size());
  }
  return absl::StrCat(type_name, " ", name);
}

std::string ToString(TypeKey* key, std::string_view name) {
  return ToString((key != nullptr ? key->GetTypeName() : ""), name);
}

ValidatedContext::ErrorCallback*& GlobalErrorCallback() {
  static ValidatedContext::ErrorCallback* callback = nullptr;
  return callback;
}

}  // namespace

std::string ContextConstraint::ToString() const {
  std::stringstream result;
  switch (presence) {
    case kInOptional:
      result << "in-optional ";
      break;
    case kInRequired:
      result << "in-required ";
      break;
    case kOutOptional:
      result << "out-optional ";
      break;
    case kOutRequired:
      result << "out-required ";
      break;
    case kScoped:
      result << "scoped ";
      break;
  }
  result << gb::ToString(type_name, name);
  return result.str();
}

bool ValidatedContext::Assign(Context* context,
                              std::vector<ContextConstraint> constraints) {
  if (context == nullptr) {
    ReportError("Context passed to ValidatedContext was null");
    return false;
  }

  // Make sure all input requirements are met.
  for (const ContextConstraint& constraint : constraints) {
    if (constraint.presence != ContextConstraint::kInRequired &&
        constraint.presence != ContextConstraint::kInOptional) {
      continue;
    }
    if (context->Exists(constraint.name, constraint.type_key)) {
      continue;
    }
    if (constraint.presence == ContextConstraint::kInRequired) {
      ReportError(absl::StrCat("Validation failed on constraint ",
                               constraint.ToString(), ": Value is missing"));
      return false;
    } else if (!constraint.name.empty() &&
               context->NameExists(constraint.name)) {
      ReportError(absl::StrCat("Validation failed on constraint ",
                               constraint.ToString(),
                               ": Value is the wrong type"));
      return false;
    }
  }

  // All requirements are met, so attempt to complete the context and set any
  // missing optional values with defaults.
  if (!Complete()) {
    return false;
  }
  for (const ContextConstraint& constraint : constraints) {
    if (constraint.presence == ContextConstraint::kInOptional &&
        constraint.default_value.has_value() &&
        !context->Exists(constraint.name, constraint.type_key)) {
      CHECK_EQ(constraint.any_type->Key(), constraint.type_key);
      context->SetAny(constraint.name, constraint.any_type,
                      constraint.default_value);
    }
  }
  context_ = context;
  constraints_ = std::move(constraints);
  return true;
}

bool ValidatedContext::Assign(ValidatedContext&& context) {
  bool result = Complete();
  shared_context_ = std::move(context.shared_context_);
  context_ = context.context_;
  context.context_ = nullptr;
  constraints_ = std::move(context.constraints_);
  return result;
}

bool ValidatedContext::CanComplete(bool report_errors) const {
  if (context_ == nullptr) {
    return true;
  }

  for (const ContextConstraint& constraint : constraints_) {
    if (constraint.presence != ContextConstraint::kOutRequired &&
        constraint.presence != ContextConstraint::kOutOptional) {
      continue;
    }
    if (context_->Exists(constraint.name, constraint.type_key)) {
      continue;
    }
    if (constraint.presence == ContextConstraint::kOutRequired) {
      if (report_errors) {
        ReportError(absl::StrCat("Validation failed on constraint ",
                                 constraint.ToString(), ": Value is missing"));
      }
      return false;
    } else if (!constraint.name.empty() &&
               context_->NameExists(constraint.name)) {
      if (report_errors) {
        ReportError(absl::StrCat("Validation failed on constraint ",
                                 constraint.ToString(),
                                 ": Value is the wrong type"));
      }
      return false;
    }
  }

  return true;
}

bool ValidatedContext::Complete() {
  if (!CanComplete(true)) {
    return false;
  }

  if (context_ == nullptr) {
    return true;
  }

  // All requirements are met, so set any missing optional values with defaults
  // and clear all scoped values.
  for (const ContextConstraint& constraint : constraints_) {
    if (constraint.presence == ContextConstraint::kOutOptional) {
      if (constraint.default_value.has_value() &&
          !context_->Exists(constraint.name, constraint.type_key)) {
        CHECK_EQ(constraint.any_type->Key(), constraint.type_key);
        context_->SetAny(constraint.name, constraint.any_type,
                         constraint.default_value);
      }
    } else if (constraint.presence == ContextConstraint::kScoped) {
      context_->Clear(constraint.name, constraint.type_key);
    }
  }

  context_ = nullptr;
  constraints_.clear();
  return true;
}

bool ValidatedContext::CanReadValue(std::string_view name, TypeKey* key) const {
  if (context_ == nullptr) {
    // Error was reported at construction or assignment.
    return false;
  }
  for (const ContextConstraint& constraint : constraints_) {
    if ((key == nullptr || constraint.type_key == key) &&
        constraint.name == name) {
      return true;
    }
  }
  ReportError(absl::StrCat("Attempt to read from ", ToString(key, name)));
  return false;
}

bool ValidatedContext::CanWriteValue(std::string_view name,
                                     TypeKey* key) const {
  if (context_ == nullptr) {
    // Error was reported at construction or assignment.
    return false;
  }
  for (const ContextConstraint& constraint : constraints_) {
    if ((key != nullptr && constraint.type_key != key) ||
        constraint.name != name) {
      continue;
    }
    if (constraint.presence == ContextConstraint::kOutOptional ||
        constraint.presence == ContextConstraint::kOutRequired ||
        constraint.presence == ContextConstraint::kScoped) {
      return true;
    }
  }
  ReportError(absl::StrCat("Attempt to write to ", ToString(key, name)));
  return false;
}

void ValidatedContext::ReportError(const std::string& message) const {
  auto global_callback = GlobalErrorCallback();
  if (global_callback != nullptr) {
    (*global_callback)(message);
    return;
  }
#ifdef DEBUG_
  LOG(FATAL) << message;
#else
  LOG(ERROR) << message;
#endif
}

void ValidatedContext::SetGlobalErrorCallback(ErrorCallback callback) {
  auto& global_callback = GlobalErrorCallback();
  delete global_callback;
  global_callback = new ErrorCallback(std::move(callback));
}

}  // namespace gb
