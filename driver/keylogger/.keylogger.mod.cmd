savedcmd_/home/miksiu/driver/keylogger/keylogger.mod := printf '%s\n'   keylogger.o | awk '!x[$$0]++ { print("/home/miksiu/driver/keylogger/"$$0) }' > /home/miksiu/driver/keylogger/keylogger.mod
