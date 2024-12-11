savedcmd_/home/miksiu/driver/sender/sender.mod := printf '%s\n'   sender.o | awk '!x[$$0]++ { print("/home/miksiu/driver/sender/"$$0) }' > /home/miksiu/driver/sender/sender.mod
