make clean; make
./dfs 10001 DFS1 &
./dfs 10002 DFS2 &
./dfs 10003 DFS3 &
./dfs 10004 DFS4 &
sleep 5
./dfc dfc.conf
pkill dfs > /dev/null
wait %2 %3
echo "Log file 1: "
cat DFS1/log.txt
echo "Log file 2: "
cat DFS2/log.txt
echo "Log file 3: "
cat DFS3/log.txt
echo "Log file 4: "
cat DFS4/log.txt
