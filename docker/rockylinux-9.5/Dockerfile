FROM rockylinux/rockylinux:9.5

WORKDIR /root

RUN dnf install -y \
    git \
    cmake \
    g++ \
    libasan \
    libubsan \
    boost-devel \
    ncurses-devel \
    libcap-devel \
    wget \
    which

# Installing libs for static build;
# Note: nothing there for libcap ... :/
#RUN dfn --enablerepo=crb \
#    glibc-static \
#    libstdc++-static \
#    boost-static


# no rpms for the following packages in the repo:
#    bats
#    bats-assert
#    bats-file
#    lcov

# Installing bats and libraries based on documentation and their Dockerfile
RUN git clone https://github.com/bats-core/bats-core.git
WORKDIR bats-core
RUN ./install.sh /opt/bats
RUN ln -s /opt/bats/bin/bats /usr/local/bin/bats
RUN ./docker/install_libs.sh support 0.3.0
RUN ./docker/install_libs.sh assert 2.1.0
RUN ./docker/install_libs.sh file 0.4.0


WORKDIR /ws

COPY entrypoint.sh .
RUN chmod +x ./entrypoint.sh

ENTRYPOINT ["./entrypoint.sh"]
