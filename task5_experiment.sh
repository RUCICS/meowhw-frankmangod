#!/bin/bash

# 本脚本使用 'dd' 命令测量不同块大小下的 I/O 吞吐量，以找到一个最优的缓冲区大小。
# 它从 /dev/zero (一个无限的零字节源) 读取数据，并写入到 /dev/null (一个黑洞)，
# 因此测量结果不受限于物理磁盘速度，而是更多地反映了系统调用的开销和内存带宽。

echo "BufferSize,Throughput(MB/s)"

# 我们测试从 1K 到 4M 的一系列块大小
for size in 1K 2K 4K 8K 16K 32K 64K 128K 256K 512K 1M 2M 4M; do
    # 为了获得稳定的测量结果，我们为每个块大小测试都传输总计 1GB 的数据。
    # dd 命令的 count 参数根据块大小动态计算。
    if [[ $size == *K ]]; then
        bs_bytes=$(echo $size | sed 's/K//' | awk '{print $1 * 1024}')
    elif [[ $size == *M ]]; then
        bs_bytes=$(echo $size | sed 's/M//' | awk '{print $1 * 1024 * 1024}')
    else
        bs_bytes=$(echo $size | sed 's/B//')
    fi
    # 总数据量为 1GB
    total_bytes=1073741824
    count=$((total_bytes / bs_bytes))
    if [ $count -eq 0 ]; then
        count=1
    fi

    # 运行 dd 并捕获其 stderr 输出，其中包含速度信息。
    # dd 的输出格式示例: "1073741824 bytes (1.1 GB, 1.0 GiB) copied, 0.28133 s, 3.8 GB/s"
    output=$(dd if=/dev/zero of=/dev/null bs=$size count=$count 2>&1)

    # 从输出中提取传输速率，例如 "3.8 GB/s"
    rate_str=$(echo "$output" | grep 'bytes' | awk '{print $(NF-1), $NF}')
    
    # 提取数值和单位
    rate_val=$(echo "$rate_str" | awk '{print $1}')
    rate_unit=$(echo "$rate_str" | awk '{print $2}')
    
    # 将所有速率单位统一转换为 MB/s，方便比较
    rate_mbs=$rate_val
    if [[ $rate_unit == "GB/s" ]]; then
        rate_mbs=$(awk -v val="$rate_val" 'BEGIN { print val * 1024 }')
    elif [[ $rate_unit == "kB/s" ]] || [[ $rate_unit == "KB/s" ]]; then
        rate_mbs=$(awk -v val="$rate_val" 'BEGIN { print val / 1024 }')
    fi
    
    # 去掉可能存在的小数点
    rate_mbs_int=$(printf "%.0f\n" "$rate_mbs")

    echo "$size,$rate_mbs_int"
done 