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
  LD_LIBRARY_PATH=${VULKAN_SDK}/lib/VulkanLoader/lib \
  VK_ADD_LAYER_PATH=${VULKAN_SDK}/share/vulkan/explicit_layer.d \
  PKG_CONFIG_PATH=${VULKAN_SDK}/lib/VulkanLoader/lib/pkgconfig:${VULKAN_SDK}/share/pkgconfig:${VULKAN_SDK}/lib/pkgconfig \
  CMAKE_PREFIX_PATH=${VULKAN_SDK}:${VULKAN_SDK}/lib/VulkanLoader
