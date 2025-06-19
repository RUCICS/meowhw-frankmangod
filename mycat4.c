#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>

// 辅助函数，确保将缓冲区内的所有字节写入文件描述符。
ssize_t write_all(int fd, const void *buf, size_t count) {
    size_t bytes_left = count;
    const char *ptr = buf;
    while (bytes_left > 0) {
        ssize_t bytes_written = write(fd, ptr, bytes_left);
        if (bytes_written == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        bytes_left -= bytes_written;
        ptr += bytes_written;
    }
    return count;
}

// 分配一个按内存页对齐的内存块。
void *align_alloc(size_t size) {
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        perror("sysconf");
        return NULL;
    }
    size_t alloc_size = size + page_size - 1 + sizeof(void *);
    void *p1 = malloc(alloc_size);
    if (p1 == NULL) return NULL;
    void *p2 = (void *)(((uintptr_t)p1 + sizeof(void *) + page_size - 1) & ~(page_size - 1));
    ((void **)p2)[-1] = p1;
    return p2;
}

// 释放由 align_alloc 分配的内存。
void align_free(void *ptr) {
    if (ptr == NULL) return;
    free(((void **)ptr)[-1]);
}

// 计算最大公约数 (GCD)
size_t gcd(size_t a, size_t b) {
    while (b) {
        a %= b;
        size_t temp = a;
        a = b;
        b = temp;
    }
    return a;
}

// 计算最小公倍数 (LCM)
size_t lcm(size_t a, size_t b) {
    if (a == 0 || b == 0) return 0;
    // 防止溢出，先除后乘
    return (a / gcd(a, b)) * b;
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // --- 确定缓冲区大小 ---
    // 1. 获取文件的状态信息
    struct stat st;
    if (stat(argv[1], &st) == -1) {
        perror("stat");
        exit(EXIT_FAILURE);
    }

    // 2. 获取文件系统的块大小
    size_t file_blk_size = st.st_blksize;
    // 任务要求提到，文件系统可能给出虚假的块大小。我们进行健全性检查。
    // 如果块大小过小或过大（不是2的次幂也可能是一个问题），我们使用一个更合理的值。
    // 这里我们简单地设置一个上下限。
    if (file_blk_size < 512 || file_blk_size > 1024 * 1024) {
        file_blk_size = 4096; // 如果大小不合理，回退到一个安全值
    }

    // 3. 获取内存页大小
    long page_size_long = sysconf(_SC_PAGESIZE);
    if (page_size_long == -1) {
        perror("sysconf");
        page_size_long = 4096; // 若获取失败，则使用一个常见默认值
    }
    size_t page_size = page_size_long;

    // 4. 计算最终的缓冲区大小
    // 我们希望缓冲区大小既是页大小的倍数，又是文件块大小的倍数，以实现最高效率。
    // 因此，我们使用它们的最小公倍数(LCM)。
    size_t buffer_size = lcm(page_size, file_blk_size);

    // 再次进行健全性检查，防止LCM计算出的值过大。
    // GNU cat 默认的缓冲区大小是 128K。我们这里也设置一个上限。
    if (buffer_size > 128 * 1024) {
        buffer_size = 128 * 1024;
    }
    // ----------------------

    // 使用我们自定义的对齐分配函数来创建缓冲区
    char *buffer = align_alloc(buffer_size);
    if (buffer == NULL) {
        perror("align_alloc");
        exit(EXIT_FAILURE);
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        align_free(buffer);
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, buffer_size)) > 0) {
        if (write_all(STDOUT_FILENO, buffer, bytes_read) == -1) {
            perror("write_all");
            align_free(buffer);
            close(fd);
            exit(EXIT_FAILURE);
        }
    }

    if (bytes_read == -1) {
        perror("read");
        align_free(buffer);
        close(fd);
        exit(EXIT_FAILURE);
    }

    align_free(buffer);
    if (close(fd) == -1) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
} 