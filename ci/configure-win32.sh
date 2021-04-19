#!/bin/sh
BASE=$1
INSTALL=$(realpath $2)

export PKG_CONFIG_PATH_CUSTOM=$INSTALL/lib/pkgconfig/
# -static
$BASE/configure \
    --prefix=$INSTALL \
    --bindir=$INSTALL/bin \
    --cross-prefix=x86_64-w64-mingw32- \
    --target-list=arm-softmmu,avr-softmmu \
    --disable-docs \
    --disable-guest-agent \
    --disable-sdl \
    --disable-gtk \
    --disable-vnc \
    --disable-xen \
    --disable-gnutls \
    --disable-nettle \
    --disable-auth-pam \
    --disable-kvm \
    --disable-cap-ng \
    --disable-libusb \
    --disable-vhost-net    \
    --disable-vhost-crypto \
    --disable-vhost-kernel \
    --disable-vhost-user   \
    --disable-live-block-migration \
    --disable-replication \
    --disable-parallels \
    --disable-crypto-afalg \
