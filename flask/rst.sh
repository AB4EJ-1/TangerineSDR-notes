killall -9 python3 
killall -9 flask
killall -9 main
killall -9 mainctl
export LC_ALL=C.UTF-8
export LANG=C.UTF-8
python3 waitress_server.py

