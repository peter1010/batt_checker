##
# Copyright (c) 2014 Peter Leese
#
# Licensed under the GPL License. See LICENSE file in the project root for full license information.  
##

CFLAGS=-Wall -O3 -Wextra
CXXFLAGS=$(CFLAGS)

CPPFLAGS= -I$(SRCDIR)/../common -DDEBUG

RM=rm -f
CC=gcc
CXX=g++
MAKEDEPEND=gcc -M $(CPPFLAGS)
LD=gcc
#-lstdc++

OBJS= battery.o

.PHONY: all
all: batt_checker


batt_checker : $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

%.o : %.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -MMD -o $@ $<
	@cp $*.d $*.P
	@sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' -e '/^$$/ d' -e 's/$$/ :/' < $*.d >> $*.P
	@$(RM) $*.d
	@mv $*.P $*.d

%.o : %.cpp
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) -MMD -o $@ $<
	@cp $*.d $*.P
	@sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' -e '/^$$/ d' -e 's/$$/ :/' < $*.d >> $*.P
	@$(RM) $*.d
	@mv $*.P $*.d

-include $(OBJS:.o=.d)
