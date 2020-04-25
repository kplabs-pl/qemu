#!/bin/sh
BASE=$1
INSTALL=$2

$BASE/qemu/configure \
    --prefix=$INSTALL \
    --target-list=arm-softmmu \
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
    --disable-vhost-vsock  \
    --disable-vhost-scsi   \
    --disable-vhost-crypto \
    --disable-vhost-kernel \
    --disable-vhost-user   \
    --disable-live-block-migration \
    --disable-replication \
    --disable-parallels \
    --disable-sheepdog \
    --disable-crypto-afalg \
