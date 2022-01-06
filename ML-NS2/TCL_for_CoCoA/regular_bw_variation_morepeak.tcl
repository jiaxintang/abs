source regular_bw_variation_morepeak_pre.tcl
puts "select CoDel"
source bw_codel.tcl;
set accelerated_bw 0;
set stride [expr 500*1000];
set pi [expr 3.1415];

set lkc_file [open link_capacity.txt w]

for {set i 0} {[expr $i*1.0/100] < $simulationTime} {incr i} {

	set cur_time [expr $i*1.0/100];
	set x_value [expr $cur_time/10*$pi/2];
	set y_value [expr sin($x_value)];
	set accelerated_bw [expr ($y_value+1)/2*$bw_upperbound];

	puts $lkc_file "$cur_time:\t[expr $bw_upperbound-$accelerated_bw]"
	$ns at [expr 0+$cur_time] "$cbr change-rate $accelerated_bw]";
}

$ns at $simulationTime "finish"
$ns run
