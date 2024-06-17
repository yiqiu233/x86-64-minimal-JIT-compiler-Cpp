#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

#include <cstring>
#include <unistd.h>
#include <sys/mman.h>

// 定义一个结构体，用于管理可执行内存页
struct MemoryPages {
    uint8_t *mem;                   // 指向可执行内存的起始地址的指针
    size_t page_size;               // 操作系统定义的内存页大小（通常为4096字节）
    size_t pages = 0;               // 从操作系统请求的内存页数
    size_t position = 0;            // 当前未使用内存空间的位置

    // 构造函数，初始化内存页
    MemoryPages(size_t pages_requested = 1) {
        // 获取机器页面大小
        page_size = sysconf(_SC_PAGE_SIZE); 
        // 使用mmap函数分配指定页数的可执行内存，返回的内存地址保存在mem指针中
        mem = (uint8_t*) mmap(NULL, page_size * pages_requested, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS ,-1, 0);
        // 如果内存分配失败，则抛出异常
        if(mem == MAP_FAILED) {
            throw std::runtime_error("Can't allocate enough executable memory!");
        }
        pages = pages_requested; // 设置已请求的内存页数
    }

    // 析构函数，释放内存页
    ~MemoryPages() {
        munmap(mem, pages * page_size); // 使用munmap函数释放内存
    }

    // 将一个uint8_t类型的数字推送到内存中
    void push(uint8_t data) {
        check_available_space(sizeof data); // 检查是否有足够的可用空间
        mem[position] = data; // 将数据写入内存
        position++; // 更新位置指针
    }

    // 将一个函数指针推送到内存中
    void push(void (*fn)()) {
        size_t fn_address = reinterpret_cast<size_t>(fn); // 将函数指针转换为地址
        check_available_space(sizeof fn_address); // 检查是否有足够的可用空间

        // 将函数地址写入内存
        std::memcpy((mem + position), &fn_address, sizeof fn_address);
        position += sizeof fn_address; // 更新位置指针
    }

    // 将一个uint8_t类型的向量推送到内存中
    void push(const std::vector<uint8_t> &data) {
        check_available_space(data.size()); // 检查是否有足够的可用空间

        // 将向量数据写入内存
        std::memcpy((mem + position), &data[0], data.size());
        position += data.size(); // 更新位置指针
    }

    // 检查是否有足够的可用空间来推送数据到内存
    void check_available_space(size_t data_size) {
        if(position + data_size > pages * page_size) {
            throw std::runtime_error("Not enough virtual memory allocated!");
        }
    }

    // 打印已使用内存的内容
    void show_memory() {
        std::cout << "\nMemory content: " << position << "/" << pages * page_size << " bytes used\n";
        std::cout << std::hex; // 设置输出为十六进制格式
        // 遍历并打印内存内容
        for(size_t i = 0; i < position; ++i) {
            std::cout << "0x" << (int) mem[i] << " ";
            if(i % 16 == 0 && i > 0) {
                std::cout << '\n'; // 每16个字节换行
            }
        }
        std::cout << std::dec; // 恢复为十进制输出格式
        std::cout << "\n\n";
    }
};

namespace AssemblyChunks {
    // 函数前导码的字节序列，用于在生成的机器代码中设置堆栈帧
    std::vector<uint8_t> function_prologue {
        0x55,               // push rbp：将基指针（rbp）推入堆栈
        0x48, 0x89, 0xe5,   // mov rbp, rsp：将堆栈指针（rsp）的值复制到基指针（rbp）
    };

    // 函数后导码的字节序列，用于结束函数并返回
    std::vector<uint8_t> function_epilogue {
        0x5d,   // pop rbp：从堆栈中弹出值并恢复基指针（rbp）
        0xc3    // ret：返回，即弹出返回地址到rip（指令指针）并跳转到该地址
    };
}

// 全局向量，将被test()函数修改
std::vector<int> a{1, 2, 3};

// 将从生成的机器代码中调用的C++函数
void test() {
    printf("Ohhh, boy ...\n");  // 输出信息
    for(auto &e : a) {          // 遍历全局向量a
        e -= 5;                  // 将每个元素减去5
    }
}

int main() {
    // 内存页实例
    MemoryPages mp;

    // 添加函数前导码
    mp.push(AssemblyChunks::function_prologue);

    // 添加对C++函数test的调用，实际上是添加test函数的地址到机器代码中
    mp.push(0x48); mp.push(0xb8); mp.push(test); // movabs rax, <function_address>：将test函数的地址（64位绝对地址）加载到rax寄存器
    mp.push(0xff); mp.push(0xd0);                // call rax：调用位于rax寄存器中的地址的函数

    // 添加函数后导码并显示生成的代码
    mp.push(AssemblyChunks::function_epilogue);
    mp.show_memory();

    // 输出全局数据初始值
    std::cout << "Global data initial values:\n";
    std::cout << a[0] << "\t" << a[1] << "\t" << a[2] << "\n";

    // 将生成的代码地址转换为函数指针并调用该函数
    void (*func)() = reinterpret_cast<void (*)()>(mp.mem);
    func();

    // 输出调用生成的代码后全局数据的值
    std::cout << "Global data after test() was called from the generated code:\n";
    std::cout << a[0] << "\t" << a[1] << "\t" << a[2] << "\n";
}
