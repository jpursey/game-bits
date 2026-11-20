# Copyright (c) 2026 John Pursey
#
# Use of this source code is governed by an MIT-style License that can be found
# in the LICENSE file or at https://opensource.org/licenses/MIT.

# The devcontainer's image (see devcontainer.json): Ubuntu 24.04 with everything
# a Linux build of Game Bits needs, and the Vulkan SDK's environment set up for
# every process, as the SDK's setup-env.sh would. It needs an x86_64 host, as
# the SDK is only built for x86_64 Linux.
FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

COPY tools/install-linux-packages.sh /tmp/gb-tools/
RUN /tmp/gb-tools/install-linux-packages.sh && rm -rf /var/lib/apt/lists/*

ARG VULKAN_SDK_VERSION
COPY tools/install-vulkan-sdk.sh /tmp/gb-tools/
RUN /tmp/gb-tools/install-vulkan-sdk.sh /opt/vulkan-sdk "${VULKAN_SDK_VERSION}"
ENV VULKAN_SDK=/opt/vulkan-sdk/${VULKAN_SDK_VERSION}/x86_64
ENV PATH=${VULKAN_SDK}/bin:${PATH} \
  PKG_CONFIG_PATH=${VULKAN_SDK}/lib/VulkanLoader/lib/pkgconfig:${VULKAN_SDK}/share/pkgconfig:${VULKAN_SDK}/lib/pkgconfig \
  CMAKE_PREFIX_PATH=${VULKAN_SDK}:${VULKAN_SDK}/lib/VulkanLoader

# Programs find the SDK's Vulkan loader and validation layer where the system's
# would be, rather than through the environment setup-env.sh sets. So they work
# however they are started, including from the desktop, which desktop-lite
# starts through sudo, clearing the environment.
RUN echo "${VULKAN_SDK}/lib/VulkanLoader/lib" > /etc/ld.so.conf.d/000-vulkan-sdk.conf \
  && ldconfig \
  && mkdir -p /usr/local/share/vulkan/explicit_layer.d \
  && sed "s#\"library_path\": \"[^\"]*\"#\"library_path\": \"${VULKAN_SDK}/lib/libVkLayer_khronos_validation.so\"#" \
    "${VULKAN_SDK}/share/vulkan/explicit_layer.d/VkLayer_khronos_validation.json" \
    > /usr/local/share/vulkan/explicit_layer.d/VkLayer_khronos_validation.json

# The desktop's window manager settings: desktop-lite's own, with a dark style
# rather than its light one. desktop-lite only writes its own when there are
# none, so these take their place.
COPY --chown=vscode:vscode .devcontainer/fluxbox-init /home/vscode/.fluxbox/init
