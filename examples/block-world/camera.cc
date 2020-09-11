#include "camera.h"

#include "glm/ext/matrix_transform.hpp"
#include "imgui.h"

// Game includes
#include "scene_types.h"

Camera::Camera() {
  UpdateView();
  UpdateStrafe();
}

void Camera::SetPosition(const glm::vec3& position) {
  position_ = position;
  UpdateView();
}

void Camera::SetDirection(const glm::vec3& direction) {
  direction_ = glm::normalize(direction);
  UpdateView();
  UpdateStrafe();
}

void Camera::UpdateView() {
  view_ = glm::lookAt(position_, position_ + direction_, kUpAxis);
}

void Camera::UpdateStrafe() {
  strafe_ = glm::normalize(glm::cross(direction_, kUpAxis));
}

void Camera::DrawGui(const char* title) {
  if (title == nullptr) {
    title = "Camera";
  }

  ImGui::Begin(title);
  if (ImGui::InputFloat3("Position", &position_.x, 3)) {
    UpdateView();
  }
  if (ImGui::InputFloat3("Direction", &direction_.x, 3)) {
    direction_ = glm::normalize(direction_);
    UpdateView();
    UpdateStrafe();
  }
#ifdef NDEBUG
  ImGui::SliderFloat("View Distance", &view_distance_, 10.0f, 1200.0f);
#else
  ImGui::SliderFloat("View Distance", &view_distance_, 10.0f, 300.0f);
#endif
  ImGui::End();
}
