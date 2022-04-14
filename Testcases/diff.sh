cls=$1
ps=$2
for (( i=1 ; i<=$cls ; i++))
do 
   	diff -Bw ./sol${cls}_0/ans_${i}_$ps.txt ./output/op-c${i}-p$ps.txt
done
