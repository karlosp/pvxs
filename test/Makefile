TOP=..

include $(TOP)/configure/CONFIG
# cfg/ sometimes isn't correctly included due to a Base bug
# so we do here (maybe again) as workaround
include $(TOP)/configure/CONFIG_PVXS_MODULE
include $(TOP)/configure/CONFIG_PVXS_VERSION
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

# access to private headers
USR_CPPFLAGS += -I$(TOP)/src

PROD_LIBS += pvxs Com

TESTPROD_HOST += testsock
testsock_SRCS += testsock.cpp
TESTS += testsock

TESTPROD_HOST += testev
testev_SRCS += testev.cpp
TESTS += testev

TESTPROD_HOST += testudp
testudp_SRCS += testudp.cpp
TESTS += testudp

TESTPROD_HOST += testshared
testshared_SRCS += testshared.cpp
TESTS += testshared

TESTPROD_HOST += testbitmask
testbitmask_SRCS += testbitmask.cpp
TESTS += testbitmask

TESTPROD_HOST += testxcode
testxcode_SRCS += testxcode.cpp
TESTS += testxcode

TESTPROD_HOST += testtype
testtype_SRCS += testtype.cpp
TESTS += testtype

TESTPROD_HOST += testdata
testdata_SRCS += testdata.cpp
TESTS += testdata

TESTPROD_HOST += testnt
testnt_SRCS += testnt.cpp
TESTS += testnt

TESTPROD_HOST += testconfig
testconfig_SRCS += testconfig.cpp
TESTS += testconfig

TESTPROD_HOST += testpvreq
testpvreq_SRCS += testpvreq.cpp
TESTS += testpvreq

TESTPROD_HOST += testinfo
testinfo_SRCS += testinfo.cpp
TESTS += testinfo

TESTPROD_HOST += testget
testget_SRCS += testget.cpp
TESTS += testget

TESTPROD_HOST += testmon
testmon_SRCS += testmon.cpp
TESTS += testmon

TESTPROD_HOST += testput
testput_SRCS += testput.cpp
TESTS += testput

TESTPROD_HOST += testrpc
testrpc_SRCS += testrpc.cpp
TESTS += testrpc

TESTPROD_HOST += mcat
mcat_SRCS += mcat.cpp
# not a unittest

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

#===========================

include $(TOP)/configure/RULES
include $(TOP)/configure/RULES_PVXS_MODULE
#----------------------------------------
#  ADD RULES AFTER THIS LINE

