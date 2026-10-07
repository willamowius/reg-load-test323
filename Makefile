#
# Makefile
#
# Makefile for registration load tester
#

PROG		= reg-load-test
SOURCES		= reg-load-test.cxx

ifndef OPENH323DIR
OPENH323DIR=$(HOME)/h323plus
endif

include $(OPENH323DIR)/openh323u.mak

# dependencies
$(OBJDIR)/reg-load-test.o: reg-load-test.h
