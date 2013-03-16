# -*- makefile -*-


PROG = webserver

LIB_SRC  = imageaccess.c debug.c http.c query.c
DEPS = -MMD -MF $(@:.o=.d)

WARNINGS = -Wall
# The following lines makes gcc return an error on any warnings
#WARNINGS += -Werror

CFLAGS += -fstack-protector -g -std=gnu99 $(WARNINGS) $(DEPS)

LIB_OBJ = $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(LIB_SRC)))
LIB_DEP = $(patsubst %.o,%.d,$(LIB_OBJ))
LIB = filelib.a

LIBS += -lpthread -lssl

PROG_SRC = webserver.c
PROG_OBJ = $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(PROG_SRC)))
PROG_DEP = $(patsubst %.o,%.d,$(PROG_OBJ))

TMP_PATH := /usr/bin:$(PATH)
export PATH = $(TMP_PATH)


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
