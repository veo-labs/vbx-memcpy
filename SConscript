import os
Import('env', 'INSTALLDIR')

BININSTALLDIR = os.path.join( INSTALLDIR, 'bin' )
LIBINSTALLDIR = os.path.join( INSTALLDIR, 'lib' )
INCINSTALLDIR = os.path.join( INSTALLDIR, 'include' )

env = env.Clone()

env.ParseConfig('pkg-config --cflags --libs gstreamer-1.0 glib-2.0')

lib = env.SharedLibrary('vbx-memcpy', ['memcpy.S', 'memset.S', 'memcpy_neon.S', 'memcpy_arm.S', 'vbx-memcpy.c'])

env.Install( LIBINSTALLDIR, lib)
