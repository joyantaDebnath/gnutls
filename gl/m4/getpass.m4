# getpass.m4 serial 4
dnl Copyright (C) 2002-2003 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

# Provide a getpass() function if the system doesn't have it.
AC_DEFUN([gl_FUNC_GETPASS],
[
  AC_REPLACE_FUNCS(getpass)
  AC_CHECK_DECLS_ONCE(getpass)
  if test $ac_cv_func_getpass = no; then
    gl_PREREQ_GETPASS
  fi
])

# Provide the GNU getpass() implementation. It supports passwords of
# arbitrary length (not just 8 bytes as on HP-UX).
AC_DEFUN([gl_FUNC_GETPASS_GNU],
[
  AC_CHECK_DECLS_ONCE(getpass)
  dnl TODO: Detect when GNU getpass() is already found in glibc.
  AC_LIBOBJ(getpass)
  gl_PREREQ_GETPASS
  dnl We must choose a different name for our function, since on ELF systems
  dnl an unusable getpass() in libc.so would override our getpass() if it is
  dnl compiled into a shared library.
  AC_DEFINE([getpass], [gnu_getpass],
    [Define to a replacement function name for getpass().])
])

# Prerequisites of lib/getpass.c.
AC_DEFUN([gl_PREREQ_GETPASS], [
  AC_CHECK_HEADERS_ONCE(stdio_ext.h)
  AC_CHECK_DECLS_ONCE([fflush_unlocked flockfile fputs_unlocked funlockfile putc_unlocked])
  :
])
