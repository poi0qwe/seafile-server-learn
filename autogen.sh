#!/bin/bash
# Run this to generate all the initial makefiles, etc.

# 前置软件
: ${AUTOCONF=autoconf}
: ${AUTOHEADER=autoheader}
: ${AUTOMAKE=automake}
: ${ACLOCAL=aclocal}
if test "$(uname)" != "Darwin"; then
    : ${LIBTOOLIZE=libtoolize}
else
    : ${LIBTOOLIZE=glibtoolize}
fi
: ${INTLTOOLIZE=intltoolize}
: ${LIBTOOL=libtool}


# 进入目录
srcdir=`dirname $0` # 令src为当前shell文件所在的目录
test -z "$srcdir" && srcdir=. # 如果srcdir为空，则令srcdir=.(当前文件夹)

ORIGDIR=`pwd` # 暂存当前工作目录
cd $srcdir # 进入当前工作目录
PROJECT=ccnet
TEST_TYPE=-f
FILE=net/main.c
CONFIGURE=configure.ac

DIE=0

# 前置检测
($AUTOCONF --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoconf installed to compile $PROJECT."
	echo "Download the appropriate package for your distribution,"
	echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
	DIE=1
}

(grep "^AC_PROG_INTLTOOL" $srcdir/$CONFIGURE >/dev/null) && {
  ($INTLTOOLIZE --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "You must have \`intltoolize' installed to compile $PROJECT."
    echo "Get ftp://ftp.gnome.org/pub/GNOME/stable/sources/intltool/intltool-0.22.tar.gz"
    echo "(or a newer version if it is available)"
    DIE=1
  }
}

($AUTOMAKE --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have automake installed to compile $PROJECT."
	echo "Get ftp://sourceware.cygnus.com/pub/automake/automake-1.7.tar.gz"
	echo "(or a newer version if it is available)"
	DIE=1
}

if test "$(uname)" != "Darwin"; then
(grep "^AC_PROG_LIBTOOL" $CONFIGURE >/dev/null) && {
  ($LIBTOOL --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "**Error**: You must have \`libtool' installed to compile $PROJECT."
    echo "Get ftp://ftp.gnu.org/pub/gnu/libtool-1.4.tar.gz"
    echo "(or a newer version if it is available)"
    DIE=1
  }
}
fi


if grep "^AM_[A-Z0-9_]\{1,\}_GETTEXT" "$CONFIGURE" >/dev/null; then
  if grep "sed.*POTFILES" "$CONFIGURE" >/dev/null; then
    GETTEXTIZE=""
  else
    if grep "^AM_GLIB_GNU_GETTEXT" "$CONFIGURE" >/dev/null; then
      GETTEXTIZE="glib-gettextize"
      GETTEXTIZE_URL="ftp://ftp.gtk.org/pub/gtk/v2.0/glib-2.0.0.tar.gz"
    else
      GETTEXTIZE="gettextize"
      GETTEXTIZE_URL="ftp://alpha.gnu.org/gnu/gettext-0.10.35.tar.gz"
    fi

    $GETTEXTIZE --version < /dev/null > /dev/null 2>&1
    if test $? -ne 0; then
      echo
      echo "**Error**: You must have \`$GETTEXTIZE' installed to compile $PKG_NAME."
      echo "Get $GETTEXTIZE_URL"
      echo "(or a newer version if it is available)"
      DIE=1
    fi
  fi
fi

# 前置检测失败，退出
if test "$DIE" -eq 1; then
	exit 1
fi


# 生成GNU配置
dr=`dirname .`
echo processing $dr
aclocalinclude="$aclocalinclude -I m4"

if test x"$MSYSTEM" = x"MINGW32"; then
    aclocalinclude="$aclocalinclude -I /mingw32/share/aclocal"
elif test "$(uname)" = "Darwin"; then
    aclocalinclude="$aclocalinclude -I /opt/local/share/aclocal"
fi

# 编译
# 1. 编译为m4文件
echo "Creating $dr/aclocal.m4 ..."
test -r $dr/aclocal.m4 || touch $dr/aclocal.m4
echo "Running glib-gettextize...  Ignore non-fatal messages."
echo "no" | glib-gettextize --force --copy

echo "Making $dr/aclocal.m4 writable ..."
test -r $dr/aclocal.m4 && chmod u+w $dr/aclocal.m4

# 2. 国际化工具
echo "Running intltoolize..."
intltoolize --copy --force --automake

# 3. 通用库工具
echo "Running $LIBTOOLIZE..."
$LIBTOOLIZE --force --copy

# 4. 开始编译
echo "Running $ACLOCAL $aclocalinclude ..."
$ACLOCAL $aclocalinclude

echo "Running $AUTOHEADER..."
$AUTOHEADER

echo "Running $AUTOMAKE --gnu $am_opt ..."
$AUTOMAKE --add-missing --gnu $am_opt

# 5. 自动配置
echo "Running $AUTOCONF ..."
$AUTOCONF
