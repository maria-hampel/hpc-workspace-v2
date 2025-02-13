FROM rockylinux/rockylinux:9.5

WORKDIR /root

RUN dnf install -y \
    git \
    cmake \
    g++ \
    boost-devel \
    wget \
    which

# do not need the following for release builds
#    libasan \
#    libubsan \

# do not install the following or cmake will try to build with it but fails
# due to missing static versions of the librarires
#    ncurses-devel \
#    libcap-devel \

# Installing libs for static build;
RUN dnf --enablerepo=crb install -y \
    glibc-static \
    libstdc++-static \
    boost-static

