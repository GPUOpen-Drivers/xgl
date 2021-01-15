#
# Dockerfile for public XGL CI with GitHub Actions.
# Sample invocation:
#    docker build . --file tools/cache_creator/docker/check_xgl.Dockerfile                    \
#                   --build-arg AMDVLK_IMAGE=gcr.io/stadia-open-source/amdvlk:nightly         \
#                   --build-arg XGL_REPO_NAME=GPUOpen-Drivers/xgl                             \
#                   --build-arg XGL_REPO_REF=<GIT_REF>                                        \
#                   --build-arg XGL_REPO_SHA=<GIT_SHA>                                        \
#                   --tag xgl-ci/xgl
#
# Required arguments:
# - AMDVLK_IMAGE: Base image name for prebuilt amdvlk
# - XGL_REPO_NAME: Name of the xgl repository to clone
# - XGL_REPO_REF: ref name to checkout
# - XGL_REPO_SHA: SHA of the commit to checkout
#

# Resume build from the specified image.
ARG AMDVLK_IMAGE
FROM "$AMDVLK_IMAGE"

ARG XGL_REPO_NAME
ARG XGL_REPO_REF
ARG XGL_REPO_SHA

# Use bash instead of sh in this docker file.
SHELL ["/bin/bash", "-c"]

# Sync the repos. Replace the base LLPC with a freshly checked-out one.
RUN cat /vulkandriver/build_info.txt \
    && (cd /vulkandriver && repo sync -c --no-clone-bundle -j$(nproc)) \
    && git -C /vulkandriver/drivers/xgl remote add origin https://github.com/"$XGL_REPO_NAME".git \
    && git -C /vulkandriver/drivers/xgl fetch origin +"$XGL_REPO_SHA":"$XGL_REPO_REF" --update-head-ok \
    && git -C /vulkandriver/drivers/xgl checkout "$XGL_REPO_SHA"

# Build XGL targets.
WORKDIR /vulkandriver/builds/ci-build
RUN source /vulkandriver/env.sh \
    && cmake --build . \
    && cmake --build . --target amdvlk64.so cache-creator

# TODO: Run the cache-creator test suites when they get merged
