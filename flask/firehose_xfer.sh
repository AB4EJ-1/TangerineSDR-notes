drf cp --nodrf --drfprops /mnt/RAM_disk/firehose/TangerineData /mnt/RAM_disk/temp
drf mv --nodrfprops /mnt/RAM_disk/firehose/TangerineData /mnt/RAM_disk/temp
rsync -ratlzv --remove-source-files --rsh="/usr/bin/sshpass -p odroid ssh -o StrictHostKeyChecking=no -l N12345" /mnt/RAM_disk/temp  N12345@192.168.1.67:/home/N12345/TangerineData

