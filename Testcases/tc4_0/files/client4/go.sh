# !/bin/bash

sed -i '25s/.*/    mean_score_factor: float = 0.8/' ./strangefish/strategies/multiprocessing_strategies.py
sed -i '26s/.*/    min_score_factor: float = 0.1/' ./strangefish/strategies/multiprocessing_strategies.py
sed -i '27s/.*/    max_score_factor: float = 0.1/' ./strangefish/strategies/multiprocessing_strategies.py

/bin/bash ./run.sh 20 3 0 f801010

sed -i '25s/.*/    mean_score_factor: float = 0.7/' ./strangefish/strategies/multiprocessing_strategies.py
sed -i '26s/.*/    min_score_factor: float = 0.1/' ./strangefish/strategies/multiprocessing_strategies.py
sed -i '27s/.*/    max_score_factor: float = 0.2/' ./strangefish/strategies/multiprocessing_strategies.py

/bin/bash ./run.sh 20 3 0 f701020

sed -i '25s/.*/    mean_score_factor: float = 0.75/' ./strangefish/strategies/multiprocessing_strategies.py
sed -i '26s/.*/    min_score_factor: float = 0.1/' ./strangefish/strategies/multiprocessing_strategies.py
sed -i '27s/.*/    max_score_factor: float = 0.15/' ./strangefish/strategies/multiprocessing_strategies.py

/bin/bash ./run.sh 20 3 0 f751015