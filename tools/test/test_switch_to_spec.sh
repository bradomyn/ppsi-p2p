#!/bin/bash

# This script is intended to repeatedly test a link between a WR switch
# port, acting as a master, and a SPEC card mounted on the same PC where
# the script runs. It checks the proper synchronization condition by
# watching the SPEC output on USB serial, provided that a USB cable is
# connected between the SPEC serial and one port of the PC, and its
# /dev/ttyUSBX is correctly detected. It simulates the condition of
# link-up/link-down by reloading the SPEC fw (fpga binary) and sw (ppsi
# binary).
#
# The test assumes that the sw running on spec is ppsi, commit 25108f41
# or more recent, running an init script which sets servo verbosity to
# 2 (more verbosity flags can help in detecting error conditions but are
# not mandatory). It relies on 'wr_servo state: " log prints.
# This is my init script:
#
# mac set 14:34:56:43:21:21
# mode slave
# verbose 1122
# ptp start
#
# One more assumption is that you have spec-fwloader and spec-cl tools
# correctly working and placed in your root user $PATH.
#
# At each iteration, these steps are executed:
# - launch minicom on the proper USB port, saving the log in /tmp
# - reload SPEC fw and sw
# - wait a random time in the range 60s-90s
# - stop minicom
# - check the minicom output for the following relevant data:
#     1) the last servo_status must be TRACK_PHASE
#     2) there must not be more than 12 servo_status different than
#        TRACK_PHASE
#     3) there must be at least 20 TRACK_PHASE
# - if condition 1) is not true, the test is marked as "Failed";
# - if condition 2) or 3) are not true, the test is marked as "Suspicious";
# - if test was Failed or Suspicious, its log is copied into OUTPUT_DIR
#   directory, otherwise it is deleted
# - whatever the outcome of the test, if a Faulty state was detected
#   during the sync process, the log is copied as well.
#
# NOTES:
# * The above conditions do not assure a complete check of what happened
# during each iteration, since they do not trace completely the
# servo_status, but only perform an euristic check. This test assumes
# that, once the TRACK_PHASE is reached and a tracking is executed 20
# times, the situation is stable and would not change. A duration test
# (check that TRACK_PHASE is stable over hours and days, with no changes on
# link status) is not the target of this script.
#
# * The tresholds (e.g. 20 TRACK_PHASEs) and timings are defined below in
# script vars and should be adjusted according to the interval of SYNC and
# DELAY_REQ/DELAY_RESP messages, and to the desired strictness.
#
# * No checks are performed on return values during script execution.
#
# IMPORTANT NOTE:
#
# * You shall not have any minicom or other communication programs
# connected to the ttyUSB you are using. If you want to know what is
# happening on your SPEC, just type tail -F /tmp/switch-test.log
#

if [ $# -lt 3 ]; then
	echo "Usage:"
	echo "$0 <spec_usb_serial_port> <spec_pci_bus_number> <num_iter>"
	echo "Example: $0 /dev/ttyUSB0 5 1200"
	exit 1
fi

# System configuration vars (read from cmdline)
SPEC_USB=$1
SPEC_PCI_BUS=$2
NUM_ITERATIONS=$3

# Tuning vars
WORKING_DIR=.
INTVL_FIX=60   # Fixed part of iteration duration
INTVL_RAND=30  # Range for random part of iteration duration
MAX_NOTRACK=12 # Max number of "no TRACK_PHASE" allowed
MIN_TRACK=20   # Min number of "TRACK_PHASE" requested
TMP_SPEC_LOG=/tmp/switch-test.log
ST_MATCH_STR='wr_servo state'
ST_OK_MATCH_STR='TRACK_PHASE'

CURDATE=$(date +%Y.%m.%d-%H.%M.%S)
OUTPUT_DIR=$WORKING_DIR/test_output_$CURDATE
TEST_LOG_FILE=$OUTPUT_DIR/tests.log
RESULT_FILE=$OUTPUT_DIR/result.csv

mkdir -p $OUTPUT_DIR

echo "Saving test result in $OUTPUT_DIR"
echo "Testing $NUM_ITERATIONS iterations. Started at $CURDATE" \
	>> $RESULT_FILE
echo -n "md5sum wrc.bin: " >> $RESULT_FILE
md5sum $WORKING_DIR/wrc.bin | cut -d' ' -f 1 >> $RESULT_FILE
echo -n "md5sum spec_top.bin: " >> $RESULT_FILE
md5sum $WORKING_DIR/spec_top.bin | cut -d' ' -f 1 >> $RESULT_FILE

echo "iteration;sleep;final_status;#no_track_status;#track_status;outcome"\
	>> $RESULT_FILE

# Kill bg cat when I am killed
killbg() {
	kill -9 $1
	echo "Test killed at $(date +%Y.%m.%d-%H.%M.%S)" >>\
		$RESULT_FILE
	exit 2
}

for i in `seq 1 $NUM_ITERATIONS` ; do

	# Load spec fw, so that previous running stops immediately
	sudo spec-fwloader -b $SPEC_PCI_BUS $WORKING_DIR/spec_top.bin

	# Init spec and its serial output
	cat $SPEC_USB > $TMP_SPEC_LOG &
	trap "killbg $!" INT

	# Run spec sw
	sudo spec-cl -b $SPEC_PCI_BUS $WORKING_DIR/wrc.bin

	# Wait for test to complete
	SLEEP_TIME=$(( $INTVL_FIX + ($RANDOM % $INTVL_RAND) ))
	echo "Iteration $i running for ${SLEEP_TIME}s..."
	sleep $SLEEP_TIME

	# Kill cat background process
	echo "Killing background cat"
	kill $!
	echo "Iteration $i done, processing results"

	# Gather and print some minimal statistic from log file
	LAST_STAT=$(grep --binary-files=text "$ST_MATCH_STR" $TMP_SPEC_LOG|\
		tail -1|cut -f4 -d' '| tr -d [[:space:]])

	NUM_NOTRACK=$(grep --binary-files=text "$ST_MATCH_STR" $TMP_SPEC_LOG|\
		grep -c -v $ST_OK_MATCH_STR)

	NUM_TRACK=$(grep --binary-files=text "$ST_MATCH_STR" $TMP_SPEC_LOG|\
		grep -c $ST_OK_MATCH_STR)

	NUM_FAULTY=$(grep -c --binary-files=text Faulty $TMP_SPEC_LOG)

	echo "Servo Status stat: last=$LAST_STAT " \
		"num_track=$NUM_TRACK num_no_track=$NUM_NOTRACK"
	IT_STATUS='failed'

	# Decide whether the test has passed or not
	if [ "$LAST_STAT" = $ST_OK_MATCH_STR ]; then
		if [ $NUM_NOTRACK -le $MAX_NOTRACK -a $NUM_TRACK -ge $MIN_TRACK ]
		then
			IT_STATUS='success'
		else
			IT_STATUS='suspicious'
		fi
	fi

	# Save log file in case of unsuccess
	if [ "$IT_STATUS" != 'success' ]; then
		cp $TMP_SPEC_LOG "$OUTPUT_DIR/${IT_STATUS}_iteration_$i"
	fi

	# Save log file in case a Faulty state was temporarly detected
	# Log file is saved even if test was successful.
	# If suspicious or failed, the same iteration can be saved twice...
	# it's suboptimal, should be fixed
	if [ $NUM_FAULTY -gt 0 ]; then
		cp $TMP_SPEC_LOG "$OUTPUT_DIR/faulty_state_iteration_$i"
	fi

	# Save iteration result to result file
	echo "$i;$SLEEP_TIME;$LAST_STAT;$NUM_NOTRACK;$NUM_TRACK;$IT_STATUS"\
		>> $RESULT_FILE
	rm -f $TMP_SPEC_LOG

done

echo "Test of $NUM_ITERATIONS ended at $(date +%Y.%m.%d-%H.%M.%S)" >>\
	$RESULT_FILE
