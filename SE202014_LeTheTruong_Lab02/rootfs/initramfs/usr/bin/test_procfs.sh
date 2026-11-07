#!/bin/sh
echo '=== LAB-02: Procfs & Sysfs Test ==='

# Step 1: Check driver module
echo '[1] Checking lab2_driver...'
if ! lsmod | grep -q '^lab2_driver'; then
    insmod /lib/modules/5.15.0/lab2_driver.ko || {
        echo 'ERROR: insmod failed!'
        exit 1
    }
else
    echo 'lab2_driver is already loaded.'
fi

# Step 2: Check device node
echo '[2] Checking /dev/lab2...'
if [ ! -c /dev/lab2 ]; then
    mknod /dev/lab2 c 240 0 || exit 1
fi
chmod 666 /dev/lab2

# Step 3: Test Procfs before writing
echo '[3] Procfs before write'
if [ -f /proc/lab2_info ]; then
    cat /proc/lab2_info
else
    echo 'ERROR: /proc/lab2_info not found!'
    exit 1
fi

# Step 4: Write data to device
echo '[4] Writing test data...'
echo 'Embedded Linux Lab2 Test Data' > /dev/lab2 || exit 1

# Step 5: Test Procfs after writing
echo '[5] Procfs after write'
cat /proc/lab2_info

# Step 6: Test Sysfs
echo '[6] Reading Sysfs attributes'
SYSFS=/sys/class/lab2_class/lab2

if [ ! -d "$SYSFS" ]; then
    echo 'ERROR: Sysfs device directory not found!'
    exit 1
fi

for attr in buffer_len open_count last_data; do
    if [ -f "$SYSFS/$attr" ]; then
        echo "$attr: $(cat "$SYSFS/$attr")"
    else
        echo "ERROR: $attr not found!"
        exit 1
    fi
done

echo '=== Procfs & Sysfs Test COMPLETED ==='
