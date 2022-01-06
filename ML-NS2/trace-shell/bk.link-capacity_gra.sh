#!/bin/sh
#!/bin/bash

cat ${1} | awk 'NR==1{tt_counter=0; last=0; next}
{
	if(($1+0)>last) {
		tt_counter = tt_counter + 1;
		tt[tt_counter]=$1; last=$1;
		num[tt_counter] = 1;
	}else if(($1+0)==last){
		num[tt_counter] = num[tt_counter] + 1;
	}
} END{ for(key in tt) { print tt[key]/1000.0 "\t" num[key];} }' > ${1}.txt
cat ${1}.txt | sort -n -k1 > ${1}.temp.txt
rm ${1}.txt

cat ${1}.temp.txt | awk 'NR==1{ bw[tt]=0; gra=0.01; tt=gra; counter=0; next}
{
	if( ($1+0)>(tt-gra) && ($1+0)<=tt ){
		
		bw[tt]=bw[tt]+$2;
	}else if( ($1+0)>tt && ($1+0)>(tt+gra) ){

		bw[tt]=0;
		tt = tt+gra;
	}else if( ($1+0)>tt && ($1+0) <= tt+gra ){

		bw[tt]=bw[tt]+$2;
		tt = tt+gra;
	}

} END {

	for(key in bw) {
		print key-gra "\t" bw[key]*1500.0*8/1000000/gra;
	}
} ' > ${1}.txtd

cat ${1}.txtd | awk ' NR==1{ tt = 0; max=0; next}
{
	if( ($2+0)>max){
		
		max = $2;
		tt = $1;
	}
} END{
	print "set baseline " int(max+1) ";"
	print "set lineRate " int(max+1) "Mb;"
	print "set inputLineRate " int(max+1) "Mb;"
	print "set bottleneckRate " int(max+1) "Mb;"
	print "set bgRate [expr " int(max+1) "*1000*1000];" 
}
' > ${1}_pre.tcl

cat ${1}.txtd | sort -n -k1 > ${1}.tr
rm ${1}.temp.txt
rm ${1}.txtd

#./new-trace.sh ${1}
#rm ${1}.tr
