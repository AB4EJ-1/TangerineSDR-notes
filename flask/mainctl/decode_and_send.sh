nice -n10 sort -nr -k 4,4 $1/WSPR/decoded$2.txt | awk '!seen[$1"_"$2"_"int($6)"_"$7] {print} {++seen[$1"_"$2"_"int($6)"_"$7]}' | sort -n -k 1,1 -k 2,2 -k 6,6 -o  $1/WSPR/decoded$2z.txt
nice -n9 curl -sS -m 30 -F allmept=$1/WSPR/decoded$2z.txt -F call=$3 -F grid=$4 https://wsprnet.org/meptspots.php
rm $1/WSPR/decoded$2.txt

