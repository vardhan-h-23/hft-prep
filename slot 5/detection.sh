# Save baseline
awk '{print $1, $5}' /proc/interrupts > /tmp/irq_before.txt

sudo taskset -c 4 ./server

# Diff after
awk '{print $1, $5}' /proc/interrupts > /tmp/irq_after.txt

# Show only lines where core 4's count changed
paste /tmp/irq_before.txt /tmp/irq_after.txt | \
  awk '{if ($2 != $4) print $1, "delta:", $4-$2}'