# DynZIS

## DynZIS is a small package to help Zowe ZIS users dynamically link their ZIS Plugins to preserve EPL-2.0 rules for packaging.

Zowe 2.0 adds 64 bit code and dynamic linking to its server-side coding
libraries. Dynamic linkage is an opt-in that all users that ship their code
should choose soon for EPL license reasons. The Zowe ZIS already provides a very
capable LPA (shared code, for non-mainframes) manager for plugins. This feature
is used by DynZIS to provide a base plugin called ZWESISDL that should be
included in all ZIS installations sooner than later. This plugin exists in the
ZOWE/zss repo under `zss/build/build_dynamic.sh`. The dynamic plugin is just a
bunch of code from `zowe-common-c/c` and `zss/c` that is commonly used in ZIS
plugins and other Metal-C programs. The DynZIS plugin installs a jump vector to
all of these routines that is found by a few dereferences from R12. Almost all
dynamic linkage in z/OS works this way, or by a few dereferences from a block
with a fixed position in the PSA. Maybe someday Zowe will get a slot in the PSA
from the z/OS team. but until then R12 is the bootstrapping point. Zowe Metal-C
Code acts like LE in that it reserves R12 for a global. This global is set when
conforming TCBs (threads) are started and when inter-program calls (PC Calls)
are made. The `ZISStubGenerator` provided in this repo allows the maintenance
and extension of this dynamic base plugin.

## ZOWE/zss/h/zis/zisstubs.h

This file is *the* manifest of record for the exported symbols. The offsets
mentioned in this file are *never* to be changed. Old slots are deleted and new
slots are granted. Again, for people that are unfamiliar with dynamic (non-LE)
linkage on z/OS, it is always this way. Backward compatibility is preserved the
simple, permanent rules.

### Usage

#### Generating stubs

The Stub generator is a simple java program with no external dependencies. It
can be compiled with javac.

The exported, dynamically linked symbols that ZIS plugins and other ZIS clients
can use is enumerated in the ZOWE/zss repo under `zss/h/zis/zisstubs.h`. This
file should be sftp-ed to a local directory which has the java program.
The `ZISStubGenerator` will generate a stub file that can be linked with a ZIS
client or plugin to provide dynamic linkage with the following invocation.

`java org.zowe.zis.ZISStubGenerator asm zisstubs.h >> zisstubs.s`

The generated `zisstubs.s` assembly language file should be copied to
`zss/c/zis/zisstubs.s`

#### Generating the dynamic base plugin initialization.

There is a section of code in `zss/c/zis/zisdynamic.c` that initializes the jump
vector. Calling the following invocation generates new initialization code.

`java org.zowe.zis.ZISStubGenerator init zisstubs.h`

This generates code to standard out that should be cut and paste into the
initialization in `zisdynamic.c`. It is hundreds of lines long and pretty
obvious-looking.  
