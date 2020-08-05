echo "DATA PATH " $1
echo "TEMP PATH " $2
echo "NODE " $3
drf cp --nodrf --drfprops $1 $2
drf mv --nodrfprops $1 $2
rsync -ratlzvm --remove-source-files --rsh="/usr/bin/sshpass -p odroid ssh -o StrictHostKeyChecking=no -l $3"  $2 $3@192.168.1.67:/home/$3/TangerineData

