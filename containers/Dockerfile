FROM ubuntu:24.04

ARG KYNA_VERSION
ARG TARGETARCH
RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates curl \
    && case "$TARGETARCH" in amd64) architecture=x86_64 ;; arm64) architecture=arm64 ;; *) exit 2 ;; esac \
    && version="${KYNA_VERSION#v}" \
    && curl -fsSL "https://github.com/Up-to-code/Kyna/releases/download/v${version}/install.sh" -o /tmp/install-kyna.sh \
    && sh /tmp/install-kyna.sh --version "$version" --prefix /usr/local --no-interactive \
    && rm -f /tmp/install-kyna.sh \
    && apt-get purge -y curl \
    && apt-get autoremove -y \
    && rm -rf /var/lib/apt/lists/*

RUN useradd --create-home --uid 10001 kyna
USER kyna
WORKDIR /workspace
EXPOSE 3000
ENTRYPOINT ["ky"]
CMD ["--help"]
