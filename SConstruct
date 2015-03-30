import os

INSTALLDIR = '#' + ARGUMENTS.get('INSTALLDIR', '.install')

env = Environment(ENV=os.environ, **os.environ)

# Generic flags
env.Append( CPPFLAGS = ['-Wall'] )
env.Append( LINKFLAGS=['-Wl,-z,defs'] )   # Fail if symbols not found

SConscript( 'SConscript', exports = ['env', 'INSTALLDIR'])
