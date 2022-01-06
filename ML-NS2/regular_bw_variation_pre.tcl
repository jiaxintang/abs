set baseline 1;
set bw_upperbound [expr 1000*1000*1000]; 
set lineRate $bw_upperbound;
set inputLineRate $bw_upperbound;
set bottleneckRate $bw_upperbound;
set bgRate [expr $bw_upperbound/2];
set simulationTime 300;
