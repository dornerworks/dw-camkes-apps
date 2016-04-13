
TARGETS := gdb_test.cdl
ADL := gdb_test.camkes


gdb_CFILES = components/gdb/src/gdb.c
Sender1_CFILES = components/Sender1/src/sender1.c
Sender2_CFILES = components/Sender2/src/sender2.c
Receiver1_CFILES = components/Receiver1/src/receiver1.c
Receiver2_CFILES = components/Receiver2/src/receiver2.c
Emitter1_CFILES = components/Emitter1/src/emitter1.c
Consumer1_CFILES = components/Consumer1/src/consumer1.c
Port1_CFILES = components/Port1/src/port1.c
Port2_CFILES = components/Port2/src/port2.c

include ${SOURCE_DIR}/../../tools/camkes/camkes.mk