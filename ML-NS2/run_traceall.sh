#!/bin/sh
#!/bin/bash
BASE=${1}
MAX=${2}
for ((i=$[$BASE-$MAX]; i<$[$BASE+$MAX]; i++ ))
{
	./ns-allinone-2.35/ns-2.35/ns dcn-fix_rtt.droptail.tcl ${i}
	cat traceall_B${i}.ta | grep "^+\ " | grep "\ 1\ tcp" > thr_trace_B${i}.tr
	awk -f realThroughput.awk thr_trace_B${i}.tr > thp_B${i}.thp
	rm thr_trace_B${i}.tr
	rm traceall_B${i}.ta
	echo "$(date): Buffer size (${i}) is finished."
}
