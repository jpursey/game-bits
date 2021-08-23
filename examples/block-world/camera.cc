#include "camera.h"

#include "glm/ext/matrix_clip_space.hpp"
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
  view_up_ = glm::normalize(glm::cross(strafe_, direction_));
}

glm::mat4 Camera::CreateProjection(const gb::FrameDimensions& size) const {
  glm::mat4 projection =
      glm::perspective(fov_, size.width / static_cast<float>(size.height),
                       GetNearDistance(), view_distance_ * 2);
  projection[1][1] *= -1.0f;
  return projection;
}

glm::vec3 Camera::CreateScreenRay(const gb::FrameDimensions& size, int screen_x,
                                  int screen_y) const {
  glm::vec2 frame_size = {static_cast<float>(size.width),
                          static_cast<float>(size.height)};
  const float near_size_y = std::tan(fov_ / 2.0f) * GetNearDistance();
  const float near_size_x = near_size_y * frame_size.x / frame_size.y;
  const glm::vec3 world_x = strafe_ * near_size_x;
  const glm::vec3 world_y = -view_up_ * near_size_y;
  const float normalized_x =
      static_cast<float>(screen_x * 2) / frame_size.x - 1.0f;
  const float normalized_y =
      static_cast<float>(screen_y * 2) / frame_size.y - 1.0f;
  const glm::vec3 screen_world_position =
      position_ + direction_ * GetNearDistance() + world_x * normalized_x +
      world_y * normalized_y;
  return glm::normalize(screen_world_position - position_);
}

void Camera::DrawGui(const char* title) {
  if (title == nullptr) {
    title = "Camera";
  }

  ImGui::Begin(title);
  if (ImGui::InputFloat3("Position", &position_.x)) {
    UpdateView();
  }
  if (ImGui::InputFloat3("Direction", &direction_.x)) {
    direction_ = glm::normalize(direction_);
    UpdateView();
    UpdateStrafe();
  }
  ImGui::SliderAngle("FOV", &fov_, 5.0f, 120.0f);
#ifdef NDEBUG
  ImGui::SliderFloat("View Distance", &view_distance_, 10.0f, 1200.0f);
#else
  ImGui::SliderFloat("View Distance", &view_distance_, 10.0f, 300.0f);
#endif
  ImGui::End();
}
