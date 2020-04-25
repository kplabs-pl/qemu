FROM archlinux/base

RUN    pacman -Syy \
    && pacman -Syu  --noconfirm \
    && echo "[ownstuff]" >> /etc/pacman.conf \
    && echo "Server = https://ftp.f3l.de/~martchus/\$repo/os/\$arch" >> /etc/pacman.conf \
    && echo "Server = https://martchus.no-ip.biz/repo/arch/\$repo/os/\$arch" >> /etc/pacman.conf \
    && echo "SigLevel = PackageOptional" >> /etc/pacman.conf \
    && pacman -Syy --verbose --noconfirm mingw-w64-gcc mingw-w64-binutils base-devel git mingw-w64-pkg-config

CMD /bin/bash
