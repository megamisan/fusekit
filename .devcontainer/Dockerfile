FROM mcr.microsoft.com/devcontainers/cpp:debian11

ARG REINSTALL_CMAKE_VERSION_FROM_SOURCE="none"

# Optionally install the cmake for vcpkg
COPY ./reinstall-cmake.sh /tmp/

RUN if [ "${REINSTALL_CMAKE_VERSION_FROM_SOURCE}" != "none" ]; then \
        chmod +x /tmp/reinstall-cmake.sh && /tmp/reinstall-cmake.sh ${REINSTALL_CMAKE_VERSION_FROM_SOURCE}; \
    fi \
    && rm -f /tmp/reinstall-cmake.sh

RUN set -eux; \
    apt-get -qq update; \
    apt-get -qq upgrade -y; \
    apt-get -qq install -y --no-install-recommends \
    build-essential libfuse-dev pkg-config gdb \
    debhelper autotools-dev fuse openssh-client \
    git sudo ca-certificates \
    bash-completion locales-all; \
    rm -rf /var/lib/apt/lists/*;

# [Optional] Uncomment this section to install additional vcpkg ports.
# RUN su vscode -c "${VCPKG_ROOT}/vcpkg install <your-port-name-here>"

# [Optional] Uncomment this section to install additional packages.
# RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
#     && apt-get -y install --no-install-recommends <your-package-list-here>
