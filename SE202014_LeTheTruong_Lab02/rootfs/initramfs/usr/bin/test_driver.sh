#!/bin/sh
echo '=== LAB-02: Device Driver Test ==='

# Step 1: Check whether the module is already loaded
echo '[1] Checking lab2_driver...'
if ! lsmod | grep -q '^lab2_driver'; then
    echo 'Loading lab2_driver...'
    insmod /lib/modules/5.15.0/lab2_driver.ko || {
        echo 'ERROR: insmod failed!'
        exit 1
    }
else
    echo 'lab2_driver is already loaded.'
fi

# Step 2: Verify device node
echo '[2] Checking device node...'
if [ ! -c /dev/lab2 ]; then
    mknod /dev/lab2 c 240 0 || exit 1
fi
chmod 666 /dev/lab2
ls -la /dev/lab2

# Step 3: Test WRITE
echo '[3] Writing to device...'
echo 'Hello from userspace, LAB-02!' > /dev/lab2

# Step 4: Test READ
echo '[4] Reading from device...'
DATA=$(cat /dev/lab2)
echo "Read back: [$DATA]"

# Step 5: Multiple write/read tests
echo '[5] Multiple write/read test...'
for i in 1 2 3; do
    echo "Message_$i" > /dev/lab2
    DATA=$(cat /dev/lab2)
    echo "Read back: [$DATA]"
done

# Step 6: Kernel messages
echo '[6] Kernel messages (dmesg):'
dmesg | grep lab2 | tail -10

echo '=== Test COMPLETED ==='
