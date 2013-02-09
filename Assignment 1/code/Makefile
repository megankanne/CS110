# -*- makefile -*-

# Modified	: Haider Razvi [hrazvi]
# Date		: Mar 2009
# - Modified "PATH" and exported it to sub-makes. Currently the default
# (.cshrc) file is not the same for all users. For different users it
# differs in terms of the value of the  PATH variable it sets. This,
# on the VINE machines, causes gcc (ver4.2 located in /usr/bin) to be
# used with ld (ver2.16 located in /usr/pubsw/bin). This results in
# a compatibilty issue as gcc (ver4.2) passes down the --hash-style
# option which is not recognized by ld (ver2.16). ld (ver2.18 located
# in /usr/bin) works fine and the PATH variable is modified to use it.
# - Placed libssl-dev libraries in the cs110 class folder and modified
# the compiler search path to look for them there.

PROG =  diskimageaccess

LIB_SRC  = diskimg.c inode.c unixfilesystem.c directory.c pathname.c  chksumfile.c file.c 
DEPS = -MMD -MF $(@:.o=.d)


WARNINGS = -fstack-protector -Wall -W -Wcast-qual -Wwrite-strings -Wextra -Wno-unused

CFLAGS += -g $(WARNINGS) $(DEPS) -std=gnu99

LIB_OBJ = $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(LIB_SRC)))
LIB_DEP = $(patsubst %.o,%.d,$(LIB_OBJ))
LIB = v6fslib.a 

PROG_SRC = diskimageaccess.c 
PROG_OBJ = $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(PROG_SRC)))
PROG_DEP = $(patsubst %.o,%.d,$(PROG_OBJ))

TMP_PATH := /usr/bin:$(PATH)
export PATH = $(TMP_PATH)

LIBS += -lssl -lcrypto

all: $(PROG)


$(PROG): $(PROG_OBJ) $(LIB)
	$(CC) $(LDFLAGS) $(PROG_OBJ) $(LIB) $(LIBS) -o $@

$(LIB): $(LIB_OBJ)
	rm -f $@
	ar r $@ $^
	ranlib $@

clean::
	rm -f $(PROG) $(PROG_OBJ) $(PROG_DEP)
	rm -f $(LIB) $(LIB_DEP) $(LIB_OBJ)


.PHONY: all clean 

-include $(LIB_DEP) $(PROG_DEP)
