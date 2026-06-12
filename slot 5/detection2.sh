#!/bin/bash

echo "Capturing baseline for IRQ 139 on Core 4..."

# Save baseline specifically for IRQ 139 (Column 6 is CPU4)
awk '/^\s*139:/ {print $1, $6}' /proc/interrupts > /tmp/irq139_before.txt

echo "Starting server pinned to Core 4..."
# Run your server tightly pinned to Core 4 in the background
sudo taskset -c 4 ./server &
SERVER_PID=$!

# Wait for the load to process (adjust time as needed)
sleep 10

echo "Capturing after-load metrics..."
# Diff after
awk '/^\s*139:/ {print $1, $6}' /proc/interrupts > /tmp/irq139_after.txt

# Clean up the background server if it runs indefinitely
sudo kill $SERVER_PID 2>/dev/null

echo "─────────────────────────────────"
echo "NVMe Queue 5 (IRQ 139) Delta"
echo "─────────────────────────────────"
# Show the delta for Core 4
paste /tmp/irq139_before.txt /tmp/irq139_after.txt | \
  awk '{
      print "IRQ " $1 " | Before: " $2 " | After: " $4 " | Delta: " $4-$2
      if ($2 == $4) {
          print "\nSUCCESS: Ghost mapping confirmed! Zero interrupts fired on Core 4."
      } else {
          print "\nWARNING: Interrupts are still firing on Core 4."
      }
  }'