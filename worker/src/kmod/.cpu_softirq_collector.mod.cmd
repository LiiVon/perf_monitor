savedcmd_cpu_softirq_collector.mod := printf '%s\n'   cpu_softirq_collector.o | awk '!x[$$0]++ { print("./"$$0) }' > cpu_softirq_collector.mod
