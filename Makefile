PROG = make
OBJS =	custr.o	\
	input.o \
	make.o	\
	parse.o	\
	util.o

SRCS = $(OBJS:%.o=%.c)

CFLAGS =	-std=c99 -g
CPPFLAGS =	-D__EXTENSIONS__ -D_FILE_OFFSET_BITS=64
LDLIBS =	-lumem

CC =	gcc
CTF =	/root/ctfconvert-altexec

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(CTF) $@

clean:
	-rm $(PROG) $(OBJS)
