include $(CARMEN_HOME)/src/Makefile.conf

LINK = g++
CXXFLAGS += -std=c++0x

MODULE_NAME = CAR MODEL
MODULE_COMMENT = Car Neural and Ackeman Motion Model

IFLAGS += -I$(CARMEN_HOME)/sharedlib/libcontrol

LFLAGS += -lfann -lcontrol

SOURCES = neural_model.cpp ackerman_model.cpp
 
PUBLIC_INCLUDES = car_model.h
PUBLIC_LIBRARIES = libcarmodel.a
#PUBLIC_BINARIES =

TARGETS = libcarmodel.a  
PUBLIC_LIBRARIES_SO =

libcarmodel.a: neural_model.o ackerman_model.o

include $(CARMEN_HOME)/src/Makefile.rules