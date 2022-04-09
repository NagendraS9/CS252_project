# !/bin/bash

eval "$(/home/rbc/anaconda3/bin/conda shell.bash hook)"	 	#initialize conda 
conda activate original						#activate strangefish environment
export STOCKFISH_EXECUTABLE=/home/rbc/stockfish_14.1_linux_x64	#absolute path to stockfish exec
export PYTHONPATH='.':$PYTHONPATH;

pids=""
for ((i=0;i<$1;i++))
do
	# echo $3
	for ((j=0;j<$2;j++))
	do
		(( id = $2 * i + j))
		mkdir ./cmdOut/$4
		python scripts/example.py --white $3 --id $id > "cmdOut/$4/${id}.txt" &
		pids="$pids $!"
	done

	for pid in $pids;
	do
		wait $pid
	done
done

