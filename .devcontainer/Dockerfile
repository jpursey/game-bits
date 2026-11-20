# Copyright (c) 2026 John Pursey
#
# Use of this source code is governed by an MIT-style License that can be found
# in the LICENSE file or at https://opensource.org/licenses/MIT.

# The devcontainer's image (see devcontainer.json): Ubuntu 24.04 with everything
# a Linux build of Game Bits needs, and the Vulkan SDK's environment set up in
# every shell.
FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

ARG VULKAN_SDK_VERSION
COPY tools/install-linux-packages.sh tools/install-vulkan-sdk.sh /tmp/gb-tools/
RUN /tmp/gb-tools/install-linux-packages.sh \
  && /tmp/gb-tools/install-vulkan-sdk.sh /opt/vulkan-sdk "${VULKAN_SDK_VERSION}" \
  && echo ". /opt/vulkan-sdk/${VULKAN_SDK_VERSION}/setup-env.sh" >> /etc/bash.bashrc \
  && rm -rf /tmp/gb-tools /var/lib/apt/lists/*
