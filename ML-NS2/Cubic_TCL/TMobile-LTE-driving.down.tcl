source TMobile-LTE-driving.down_pre.tcl
if { $argc == 0 } {
	source red.tcl;
} elseif { $argc == 1 } {
	puts "select CoDel";
	source codel.tcl;
} elseif { $argc == 4} {

	if { [expr [lindex $argv 0]] == 0} {
		puts "select RED with variable flowsize"
		source red_flowsize.tcl;
	} else {
		puts "select CoDel with variable flowsize"
		source codel_flowsize.tcl;
	}
}
$ns at 0 "$cbr change-rate [expr $baseline*1000*1000-11.502*1000*1000]"
$ns at 10 "$cbr change-rate [expr $baseline*1000*1000-7.3464*1000*1000]"
$ns at 20 "$cbr change-rate [expr $baseline*1000*1000-6.5844*1000*1000]"
$ns at 30 "$cbr change-rate [expr $baseline*1000*1000-9.3828*1000*1000]"
$ns at 40 "$cbr change-rate [expr $baseline*1000*1000-6.864*1000*1000]"
$ns at 50 "$cbr change-rate [expr $baseline*1000*1000-5.9208*1000*1000]"
$ns at 60 "$cbr change-rate [expr $baseline*1000*1000-5.7312*1000*1000]"
$ns at 70 "$cbr change-rate [expr $baseline*1000*1000-14.0616*1000*1000]"
$ns at 80 "$cbr change-rate [expr $baseline*1000*1000-14.934*1000*1000]"
$ns at 90 "$cbr change-rate [expr $baseline*1000*1000-13.218*1000*1000]"
$ns at 100 "$cbr change-rate [expr $baseline*1000*1000-9.9888*1000*1000]"
$ns at 110 "$cbr change-rate [expr $baseline*1000*1000-15.75*1000*1000]"
$ns at 120 "$cbr change-rate [expr $baseline*1000*1000-12.918*1000*1000]"
$ns at 130 "$cbr change-rate [expr $baseline*1000*1000-19.5888*1000*1000]"
$ns at 140 "$cbr change-rate [expr $baseline*1000*1000-11.724*1000*1000]"
$ns at 150 "$cbr change-rate [expr $baseline*1000*1000-20.37*1000*1000]"
$ns at 160 "$cbr change-rate [expr $baseline*1000*1000-20.6676*1000*1000]"
$ns at 170 "$cbr change-rate [expr $baseline*1000*1000-14.9796*1000*1000]"
$ns at 180 "$cbr change-rate [expr $baseline*1000*1000-28.2492*1000*1000]"
$ns at 190 "$cbr change-rate [expr $baseline*1000*1000-36.3588*1000*1000]"
$ns at 200 "$cbr change-rate [expr $baseline*1000*1000-6.084*1000*1000]"
$ns at 210 "$cbr change-rate [expr $baseline*1000*1000-4.8648*1000*1000]"
$ns at 220 "$cbr change-rate [expr $baseline*1000*1000-3.9864*1000*1000]"
$ns at 230 "$cbr change-rate [expr $baseline*1000*1000-3.924*1000*1000]"
$ns at 240 "$cbr change-rate [expr $baseline*1000*1000-3.3468*1000*1000]"
$ns at 250 "$cbr change-rate [expr $baseline*1000*1000-3.6864*1000*1000]"
$ns at 260 "$cbr change-rate [expr $baseline*1000*1000-3.276*1000*1000]"
$ns at 270 "$cbr change-rate [expr $baseline*1000*1000-3.4968*1000*1000]"
$ns at 280 "$cbr change-rate [expr $baseline*1000*1000-4.0464*1000*1000]"
$ns at 290 "$cbr change-rate [expr $baseline*1000*1000-4.5828*1000*1000]"
$ns at 300 "$cbr change-rate [expr $baseline*1000*1000-5.676*1000*1000]"
$ns at 310 "$cbr change-rate [expr $baseline*1000*1000-7.7196*1000*1000]"
$ns at 320 "$cbr change-rate [expr $baseline*1000*1000-25.7052*1000*1000]"
$ns at 330 "$cbr change-rate [expr $baseline*1000*1000-20.5836*1000*1000]"
$ns at 340 "$cbr change-rate [expr $baseline*1000*1000-12.4812*1000*1000]"
$ns at 350 "$cbr change-rate [expr $baseline*1000*1000-12.3948*1000*1000]"
$ns at 360 "$cbr change-rate [expr $baseline*1000*1000-13.464*1000*1000]"
$ns at 370 "$cbr change-rate [expr $baseline*1000*1000-7.8204*1000*1000]"
$ns at 380 "$cbr change-rate [expr $baseline*1000*1000-9.6156*1000*1000]"
$ns at 390 "$cbr change-rate [expr $baseline*1000*1000-14.688*1000*1000]"
$ns at 400 "$cbr change-rate [expr $baseline*1000*1000-25.5192*1000*1000]"
$ns at 410 "$cbr change-rate [expr $baseline*1000*1000-19.0164*1000*1000]"
$ns at 420 "$cbr change-rate [expr $baseline*1000*1000-11.4372*1000*1000]"
$ns at 430 "$cbr change-rate [expr $baseline*1000*1000-25.7544*1000*1000]"
$ns at 440 "$cbr change-rate [expr $baseline*1000*1000-12.1344*1000*1000]"
$ns at 450 "$cbr change-rate [expr $baseline*1000*1000-25.3824*1000*1000]"
$ns at 460 "$cbr change-rate [expr $baseline*1000*1000-22.7688*1000*1000]"
$ns at 470 "$cbr change-rate [expr $baseline*1000*1000-6.4872*1000*1000]"
$ns at $simulationTime "finish"
$ns run
