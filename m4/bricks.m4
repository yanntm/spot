AC_DEFUN([AX_CHECK_BRICKS], [
  AC_SUBST([BRICKS_CPPFLAGS], ['-I$(top_srcdir)/bricks'])
  AC_CONFIG_SUBDIRS([bricks])
])
