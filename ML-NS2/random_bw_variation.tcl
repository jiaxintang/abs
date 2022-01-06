source regular_bw_variation_morepeak_pre.tcl
if { $argc <= 2 } {
	source bw_red.tcl;
} else {
	puts "select CoDel"
	source bw_codel.tcl;
}
set accelerated_bw 0;
set stride [expr 500*1000];
set pi [expr 3.1415];

if { $argc == 2} {
	set SEEDS [expr [lindex $argv 1]];
} elseif { $argc == 3 } {
	set SEEDS [expr [lindex $argv 2]];
} else {
	set SEEDS 0;
}
puts "SEED is $SEEDS"
set lkc_file [open link_capacity.txt w]
set rng [new RNG]
$rng seed $SEEDS
set bw_ratio [new RandomVariable/Uniform];
$bw_ratio use-rng $rng
$bw_ratio set min_ 0.001;
$bw_ratio set max_ 0.999;

for {set i 0} {[expr $i] < $simulationTime} {set i [expr $i+3]} {

	set accelerated_bw [expr $bottleneckRate*(1-[$bw_ratio value])];
	$ns at [expr 0+$i] "$cbr change-rate $accelerated_bw]";
}

$ns at $simulationTime "finish"
$ns run
