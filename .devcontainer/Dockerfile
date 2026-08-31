FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04
ARG KYNA_VERSION=1.0.0
RUN curl -fsSL "https://github.com/Up-to-code/Kyna/releases/download/v${KYNA_VERSION}/install.sh" \
    | sh -s -- --version "$KYNA_VERSION" --prefix /usr/local --no-interactive
