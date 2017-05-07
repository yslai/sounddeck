PREFIX =	/usr/local/sounddeck

PATH_A =	$(shell LC_ALL=C /usr/sbin/ldconfig -p | \
		/usr/bin/sed "/x86-64.*libDeckLinkAPI\\.so/!d;\
		s/.* => /\\\\\"/;s/$$/\\\\\"/;q")
PATH_PA =	$(shell LC_ALL=C /usr/sbin/ldconfig -p | \
		/usr/bin/sed "/x86-64.*libDeckLinkPreviewAPI\\.so/!d;\
		s/.* => /\\\\\"/;s/$$/\\\\\"/;q")

CXX =		/usr/bin/g++
DEBUG ?=	0
ifeq ($(DEBUG),1)
    CFLAGS =	-g -O1
else
    CFLAGS =	-O2 -s
endif
CFLAGS +=	-std=c++03 -Wextra -Wall -Werror \
		-Wno-unused-parameter -fPIC
CFLAGS +=	-Wno-multichar -fno-rtti
ifeq ($(findstring gcc,$(CXX)),g++)
    CFLAGS +=	-no-canonical-prefixes -Wno-builtin-macro-redefined \
		-D__DATE__="redacted" -D__TIMESTAMP__="redacted" \
		-D__TIME__="redacted" -U_FORTIFY_SOURCE \
		-D_FORTIFY_SOURCE=1 -fstack-protector
endif
CFLAGS +=	-Iinclude

SRC_A  =	api.cc
SOLIB_A =	libDeckLinkAPI.so
#ifeq ($(PATH_A),)
CDEFINES_A =
#else
#CDEFINES_A =	-DPATH_A=$(PATH_A)
#endif

SRC_PA	=	preview_api.cc
SOLIB_PA =	libDeckLinkPreviewAPI.so

SOLIB =		$(SOLIB_A) $(SOLIB_PA)

LDLIBS =	-lasound

COMPILE_A =	$(CXX) $(CFLAGS) $(CDEFINES_A) -shared -o $@ \
		$(SRC_A) $(LDLIBS)

COMPILE_PA =	$(CXX) $(CFLAGS) -shared -o $@ \
		$(SRC_PA) $(LDLIBS)

all:		$(SOLIB)

$(SOLIB_A):	$(SRC_A) $(DEP)
		$(COMPILE_A)

$(SOLIB_PA):	$(SRC_PA) $(DEP)
		$(COMPILE_PA)

clean:
		/usr/bin/rm -f $(SOLIB) *~

install:	$(SOLIB)
		/usr/bin/install -m 555 $(SOLIB) $(PREFIX)/lib64
