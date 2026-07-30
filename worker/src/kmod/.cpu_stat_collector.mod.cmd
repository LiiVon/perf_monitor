savedcmd_cpu_stat_collector.mod := printf '%s\n'   cpu_stat_collector.o | awk '!x[$$0]++ { print("./"$$0) }' > cpu_stat_collector.mod
