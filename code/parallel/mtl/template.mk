##
##  Template makefile for Standard, Profile, Debug, Release, and Release-static versions
##
##    eg: "make rs" for a statically linked release version.
##        "make d"  for a debug version (no optimizations).
##        "make"    for the standard version (optimized, but with debug information and assertions active)

##MROOT: the parent directory
##PWD: gives the current directory
##EXEC: gives the file names inside the actual directory if there is nothing in the variable
MROOT      = ../
PWD        = $(shell pwd)
EXEC      ?= $(notdir $(PWD))

##DEPDIR set in the parent file (ex. Makefile in /core)

##CSRCS: gets all the .cc in the actual directory
##DSRCS:
##CHDRS: gets all the .h in the actual directory
##COBJS:
CSRCS      = $(wildcard $(PWD)/*.cc)
DSRCS      = $(foreach dir, $(DEPDIR), $(filter-out $(MROOT)/$(dir)/Main.cc, $(wildcard $(MROOT)/$(dir)/*.cc)))
CHDRS      = $(wildcard $(PWD)/*.h)
COBJS      = $(CSRCS:.cc=.o) $(DSRCS:.cc=.o)

##PCOBJS: adds the suffix p to all COBJS
##DCOBJS: adds the suffix d to all COBJS
##RCOBJS: adds the suffix r to all COBJS
PCOBJS     = $(addsuffix p,  $(COBJS))
DCOBJS     = $(addsuffix d,  $(COBJS))
RCOBJS     = $(addsuffix r,  $(COBJS))


##CXX: sets g++ to v8 if there is nothing set
##CFLAGS:
##LFLAGS:
CXX       ?= g++-8
CFLAGS    ?= -Wall -Wno-parentheses
LFLAGS    ?= -Wall

##COPTIMIZE: sets COPTIMIZE flag to -03 if it's empty
COPTIMIZE ?= -O3

##CFLAGS:
##LFLAGS:
CFLAGS    += -I$(MROOT) -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS
LFLAGS    += -lz

KAFKA = -I/usr/include -I/usr/local/include -L/usr/local/lib -lrdkafka -lm
MONGO = -I/usr/local/include/libbson-1.0 -I/usr/local/include/libmongoc-1.0 -I/usr/include/libbson-1.0 -I/usr/include/libmongoc-1.0 -lbson-1.0 -lmongoc-1.0

.PHONY : s p d r rs clean 

s:	$(EXEC)
p:	$(EXEC)_profile
d:	$(EXEC)_debug
r:	$(EXEC)_release
rs:	$(EXEC)_static

libs:	lib$(LIB)_standard.a
libp:	lib$(LIB)_profile.a
libd:	lib$(LIB)_debug.a
libr:	lib$(LIB)_release.a

## Compile options
%.o:			CFLAGS +=$(COPTIMIZE) -g -D DEBUG -fopenmp
%.op:			CFLAGS +=$(COPTIMIZE) -pg -g -D NDEBUG -fopenmp
%.od:			CFLAGS +=-O0 -g -D DEBUG -fopenmp# -D INVARIANTS
%.or:			CFLAGS +=$(COPTIMIZE) -g -D NDEBUG -fopenmp

## Link options
$(EXEC):		LFLAGS += -g -fopenmp
$(EXEC)_profile:	LFLAGS += -g -pg -fopenmp
$(EXEC)_debug:		LFLAGS += -g -fopenmp
#$(EXEC)_release:	LFLAGS += ...
$(EXEC)_static:		LFLAGS += --static -fopenmp

## Dependencies
$(EXEC):		$(COBJS)
$(EXEC)_profile:	$(PCOBJS)
$(EXEC)_debug:		$(DCOBJS)
$(EXEC)_release:	$(RCOBJS)
$(EXEC)_static:		$(RCOBJS)

lib$(LIB)_standard.a:	$(filter-out */Main.o,  $(COBJS))
lib$(LIB)_profile.a:	$(filter-out */Main.op, $(PCOBJS))
lib$(LIB)_debug.a:	$(filter-out */Main.od, $(DCOBJS))
lib$(LIB)_release.a:	$(filter-out */Main.or, $(RCOBJS))


## Build rule
%.o %.op %.od %.or:	%.cc
##echo message
##compiler (g++) flags () kafka flags (-lrdkafka -lcppkafka) -c -o $@ $<
	@echo Compiling: $(subst $(MROOT)/,,$@)
	@$(CXX) $(CFLAGS) -c -o $@ $< $(KAFKA) $(MONGO)

## Linking rules (standard/profile/debug/release)
$(EXEC) $(EXEC)_profile $(EXEC)_debug $(EXEC)_release $(EXEC)_static:
##echo message
##compiler (g++) $^ flags () kafka flags (-lrdkafka -lcppkafka) -o $@
	@echo Linking: "$@ ( $(foreach f,$^,$(subst $(MROOT)/,,$f)) )"
	@$(CXX) $^ $(LFLAGS) -o $@ $(KAFKA) $(MONGO)

## Library rules (standard/profile/debug/release)
lib$(LIB)_standard.a lib$(LIB)_profile.a lib$(LIB)_release.a lib$(LIB)_debug.a:
##echo message
##AR? -rcsv $@ $^
	@echo Making library: "$@ ( $(foreach f,$^,$(subst $(MROOT)/,,$f)) )"
	@$(AR) -rcsv $@ $^

## Library Soft Link rule:
libs libp libd libr:
##echo message
##ln -sf $^ lib
	@echo "Making Soft Link: $^ -> lib$(LIB).a"
	@ln -sf $^ lib$(LIB).a

## Clean rule
clean:
	@rm -f $(EXEC) $(EXEC)_profile $(EXEC)_debug $(EXEC)_release $(EXEC)_static \
	  $(COBJS) $(PCOBJS) $(DCOBJS) $(RCOBJS) *.core depend.mk 

## Make dependencies
depend.mk: $(CSRCS) $(CHDRS)
	@echo Making dependencies
	@$(CXX) $(CFLAGS) -I$(MROOT) $(KAFKA) $(MONGO) \
	   $(CSRCS) -MM | sed 's|\(.*\):|$(PWD)/\1 $(PWD)/\1r $(PWD)/\1d $(PWD)/\1p:|' > depend.mk
	@for dir in $(DEPDIR); do \
	      if [ -r $(MROOT)/$${dir}/depend.mk ]; then \
		  echo Depends on: $${dir}; \
		  cat $(MROOT)/$${dir}/depend.mk >> depend.mk; \
	      fi; \
	  done

-include $(MROOT)/mtl/config.mk
-include depend.mk

##g++-10 -Wall -Wno-parentheses -I../ -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS \
	-O3 -g -D DEBUG -fopenmp -lrdkafka -lcppkafka -c -o path_to_Main.o path_to_Main.cc
## g++-10: compiler
## -Wall: all warnings
## -Wno-parentheses: 
## -I../: adds the directory ../ to the list of directory to be searched for header files
## -D:
## -O3: Optimizes the code (compilation takes more time)
## -fopenmp: allows pragma to work (multi threading) WARNING to -pthread