#!/bin/sh
#!/bin/bash

echo "source red.tcl;" > ${1}.tcl

cat ${1}.tr | awk 'NR==0{tt=0; next}
{
	print "$ns at " $1-1 " \"$cbr change-rate [expr 30*1000*1000-" $2 "*1000*1000]\""
	
} END{ }' >> ${1}.tcl

echo "\$ns at \$simulationTime \"finish\"" >> ${1}.tcl
echo "\$ns run" >> ${1}.tcl
