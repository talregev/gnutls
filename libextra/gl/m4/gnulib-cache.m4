# Copyright (C) 2002-2009 Free Software Foundation, Inc.
#
# This file is free software, distributed under the terms of the GNU
# General Public License.  As a special exception to the GNU General
# Public License, this file may be distributed as part of a program
# that contains a configuration script generated by Autoconf, under
# the same distribution terms as the rest of that program.
#
# Generated by gnulib-tool.
#
# This file represents the specification of how gnulib-tool is used.
# It acts as a cache: It is written and read by gnulib-tool.
# In projects using CVS, this file is meant to be stored in CVS,
# like the configure.ac and various Makefile.am files.


# Specification in the form of a command-line invocation:
#   gnulib-tool --import --dir=. --lib=libxgnu --source-base=gl --m4-base=gl/m4 --doc-base=doc --tests-base=gl/tests --aux-dir=build-aux --avoid=dummy --avoid=stdint --makefile-name=gnulib.mk --libtool --macro-prefix=xgl --no-vc-files crypto/hmac-md5 crypto/md5 extensions havelib

# Specification in the form of a few gnulib-tool.m4 macro invocations:
gl_LOCAL_DIR([])
gl_MODULES([
  crypto/hmac-md5
  crypto/md5
  extensions
  havelib
])
gl_AVOID([dummy stdint])
gl_SOURCE_BASE([gl])
gl_M4_BASE([gl/m4])
gl_PO_BASE([])
gl_DOC_BASE([doc])
gl_TESTS_BASE([gl/tests])
gl_LIB([libxgnu])
gl_MAKEFILE_NAME([gnulib.mk])
gl_LIBTOOL
gl_MACRO_PREFIX([xgl])
gl_PO_DOMAIN([])
gl_VC_FILES([false])
