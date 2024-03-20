require qt5.inc
require qt5-git.inc

LICENSE = "GPL-3.0-only | The-Qt-Company-Commercial"
LIC_FILES_CHKSUM = " \
    file://LICENSE.GPL3;md5=d32239bcb673463ab874e80d47fae504 \
"

DEPENDS += "qtbase qtdeclarative qtmultimedia"

SRCREV = "df56d9b4e6373b5975b6cf636ffb5ccdebe9e9fc"

# The same issue as in qtbase:
# http://errors.yoctoproject.org/Errors/Details/152641/
LDFLAGS:append:x86 = "${@bb.utils.contains('DISTRO_FEATURES', 'ld-is-gold', ' -fuse-ld=bfd ', '', d)}"

PACKAGECONFIG ?= "qtquickcompiler"
