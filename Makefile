VERSION = 3.02
## Simple generic makefile for c++ projects by Ory Chowaw-Liebman
## Takes all files with extension EXT in directory SOURCEDIR
## and uses them to build binary BIN in TARGETDIR, 
## where the object files will also be left.
##
## Provides commands all to build the binary, clean removes build directory,
## checkdirs makes sure the TARGETDIR exists and and BIN builds the binary.
## MAINFILE for the project is to be exluded from tests, which provide their own main function.
CC      = g++
EXT = cxx
SOURCEDIR = Intro
TESTDIR = IntroTest
TARGETDIR = build
BIN = intro
MAINFILE	= Intro

## Compilar and linker flags
CFLAGS  = $(shell llvm-config --cxxflags)
CFLAGS += -I"/usr/local/include" 
LLFLAGS = $(shell llvm-config --ldflags --system-libs --libs all)
LLFLAGS += -L"/usr/local/lib" -rdynamic
#LLFLAGS += -L"/usr/local/lib" -lcityhash  -rdynamic

## Making every header a depenency for a source file will rebuild all source files,
## but that is safer than not rebuilding. Otherwise, just leave empty, but then 
## you have to make clean regularly, when le wild bug pops up.
DEPS := $(foreach sdir,$(SOURCEDIR),$(wildcard $(sdir)/*.h))
TESTDEPS := $(foreach sdir,$(TESTDIR),$(wildcard $(sdir)/*.h))

## Configuration ends
#############################################
## Internals start here
## with listsof source and object files

DEPS+= $(SOURCEDIR)/Parser.h $(SOURCEDIR)/Scanner.h

TESTDEPS := $(foreach sdir,$(TESTDIR),$(wildcard $(sdir)/*.h))

## Configuration ends
#############################################
## Internals start here
## with listsof source and object files
SRC     := $(foreach sdir,$(SOURCEDIR),$(wildcard $(sdir)/*.$(EXT)))
OBJ     := $(patsubst $(SOURCEDIR)/%.$(EXT),$(TARGETDIR)/%.o,$(SRC))

OBJ += $(TARGETDIR)/Parser.o $(TARGETDIR)/Scanner.o

TESTSRC	:= $(foreach sdir,$(TESTDIR),$(wildcard $(sdir)/*.$(EXT)))
TESTOBJ	:= $(patsubst $(TESTDIR)/%.$(EXT),$(TARGETDIR)/%.o,$(TESTSRC))
TESTOBJ	+= $(filter-out $(TARGETDIR)/$(MAINFILE).o,$(OBJ))

vpath %.cxx $(SOURCEDIR)
vpath %.cpp $(SOURCEDIR)

CFLAGS  += $(addprefix -I,$(SOURCEDIR))

.PHONY: all $(TARGETDIR) clean

## build dir and binary
all: $(TARGETDIR) $(BIN)

$(TARGETDIR)/Parser.o: $(SOURCEDIR)/Parser.cpp  $(DEPS)
	@echo "X Building $<"
	$(CC) $(CFLAGS) -c $(SOURCEDIR)/Parser.cpp -o $(TARGETDIR)/Parser.o

$(TARGETDIR)/Scanner.o: $(SOURCEDIR)/Scanner.cpp $(DEPS)
	@echo "X Building $<"
	$(CC) $(CFLAGS) -c $(SOURCEDIR)/Scanner.cpp -o $(TARGETDIR)/Scanner.o

$(SOURCEDIR)/Parser.cpp: parser

$(SOURCEDIR)/Scanner.cpp: parser

$(SOURCEDIR)/Parser.h: parser

$(SOURCEDIR)/Scanner.h: parser

parser: intro.atg
	@echo "Running Coco\R..."
	#coco/coco.exe intro.atg  -namespace parse -frames coco/ -o $(SOURCEDIR)/
#intro.atg: 

## Build binary from object files
## removed parser from deps to save time
$(BIN): $(OBJ) 
	$(CC) -o $(TARGETDIR)/$(BIN) $(OBJ) $(LLFLAGS)

## Ensure target directory exists
$(TARGETDIR):
	@mkdir -p $@

test : $(TARGETDIR) test_$(BIN)

test_$(BIN): $(TESTOBJ)
	$(CC) -o $(TARGETDIR)/test_$(BIN) $(TESTOBJ) $(LLFLAGS) -lgtest -lpthread


## Clean up just removes the build dir
clean:
	@rm -rf $(TARGETDIR)

$(TARGETDIR)/%.o: $(SOURCEDIR)/%.$(EXT) $(DEPS)
	@echo "Building $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGETDIR)/%.o: $(TESTDIR)/%.$(EXT) $(DEPS) $(TESTDEPS)
	@echo "Building $<"
	$(CC) $(CFLAGS) -DTEST -c $< -o $@
