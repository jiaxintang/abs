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

cat ${1}.temp.txt | awk 'NR==1{tt=1; bw[tt]=0; counter=0; next}
{
	if( ($1+0)>(tt-1) && ($1+0)<=tt ){
		
		bw[tt]=bw[tt]+$2;
	}else if( ($1+0)>tt && ($1+0)>(tt+1) ){

		bw[tt]=0;
		tt = tt+1;
	}else if( ($1+0)>tt && ($1+0) <= tt+1 ){

		bw[tt]=bw[tt]+$2;
		tt = tt+1;
	}

} END {

	for(key in bw) {
		print key "\t" bw[key]*1500.0*8/1000000;
	}
} ' > ${1}.txtd

cat ${1}.txtd | awk ' NR==1{ tt = 0; max=0; next}
{
	if( ($2+0)>max){
		
		max = $2;
		tt = $1;
	}
} END{
	print max "Mbps @" tt;
}
'

cat ${1}.txtd | sort -n -k1 > ${1}.tr
rm ${1}.temp.txt
rm ${1}.txtd

./new-trace.sh ${1}
rm ${1}.tr
