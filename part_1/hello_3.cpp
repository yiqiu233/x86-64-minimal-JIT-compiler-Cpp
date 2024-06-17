// hello_3.cpp
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/mman.h>
/*
main函数：
获取名字。
定义了一个机器码向量machine_code，其中包含了系统调用write的指令（针对Linux和macOS有不同的系统调用号）。
调用append_message_size函数将消息大小添加到机器码中。
将问候语添加到机器码向量的末尾。
打印机器码向量。
调用estimate_memory_size函数获取足够的内存大小以存储机器码。
使用mmap函数分配可执行内存，并将机器码复制到该内存区域。
将内存地址转换为函数指针，并调用该函数。
释放映射的内存。

append_message_size函数将消息大小（字符串长度）以字节为单位添加到机器码向量的指定位置。
show_machine_code函数以十六进制格式显示机器码向量的内容。
estimate_memory_size函数获取机器页大小，并计算一个足够大的内存块，以便能容纳机器码。
*/

// 添加消息大小到机器码向量中
void append_message_size(std::vector<uint8_t> &machine_code, const std::string &hello_name);

// 打印机器码向量的内容
void show_machine_code(const std::vector<uint8_t> &machine_code);

// 返回可以存储生成机器码的机器页面大小的倍数
size_t estimate_memory_size(size_t machine_code_size);

int main()
{
    std::string name;
    std::cout << "What is your name?\n";
    std::getline(std::cin, name);
    std::string hello_name = "Hello, " + name + "!\n";

    // 在内存中存储机器码
    std::vector<uint8_t> machine_code{
#ifdef __linux__
        // 存储Linux系统调用号0x01，表示“write”操作
        0x48, 0xc7, 0xc0, 0x01, 0x00, 0x00, 0x00,
#elif __APPLE__
        // 存储macOS系统调用号0x02000004，表示“write”操作
        0x48, 0xc7, 0xc0, 0x04, 0x00, 0x00, 0x02,
#endif
        // 存储标准输入文件描述符0x01
        0x48, 0xc7, 0xc7, 0x01, 0x00, 0x00, 0x00,
        // 存储待写入字符串的位置（距离当前指令指针3个指令）
        0x48, 0x8d, 0x35, 0x0a, 0x00, 0x00, 0x00,
        // 存储字符串长度（初始化为0）
        0x48, 0xc7, 0xc2, 0x00, 0x00, 0x00, 0x00,
        // 执行系统调用
        0x0f, 0x05,
        // 返回指令
        0xc3};

    // 添加消息大小到机器码向量
    append_message_size(machine_code, hello_name);

    // 将消息追加到机器码向量中
    for (auto c : hello_name)
    {
        machine_code.push_back(c);
    }

    // 打印机器码向量的内容
    show_machine_code(machine_code);

    // 获取mmap所需的内存大小
    size_t required_memory_size = estimate_memory_size(machine_code.size());

    // 使用mmap分配内存
    uint8_t *mem = (uint8_t *)mmap(NULL, required_memory_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED)
    {
        std::cerr << "无法分配内存\n";
        std::exit(1);
    }

    // 将生成的机器码复制到可执行内存
    for (size_t i = 0; i < machine_code.size(); ++i)
    {
        mem[i] = machine_code[i];
    }

    // 将内存地址转换为函数指针并调用函数
    void (*func)();
    func = (void (*)())mem;
    func();

    // 释放映射的内存
    munmap(mem, required_memory_size);
}

// 返回可以存储生成机器码的机器页面大小的倍数
size_t estimate_memory_size(size_t machine_code_size)
{
    // 获取系统页面大小，即一个内存页面的字节数
    // sysconf(_SC_PAGE_SIZE) 是UNIX系统调用，用于获取系统配置参数
    size_t page_size_multiple = sysconf(_SC_PAGE_SIZE);
    size_t factor = 1, required_memory_size;

    for (;;)
    {
        // 计算所需的内存大小，即页面大小的倍数
        required_memory_size = factor * page_size_multiple;
        // 如果机器码大小小于等于所需内存大小，则跳出循环
        // 这样可以确保返回的内存大小是页面大小的倍数，并且足够存储机器码
        if (machine_code_size <= required_memory_size)
            break;
        factor++;
    }
    // 返回计算出的所需内存大小
    return required_memory_size;
}

// 将消息大小添加到机器码向量中
void append_message_size(std::vector<uint8_t> &machine_code, const std::string &hello_name)
{
    // 获取消息（字符串）的长度
    size_t message_size = hello_name.length();

    // 将消息大小拆分为四个字节，并依次存储到machine_code向量的第25、26、27、28个位置（数组索引从0开始）
    // 这里假设machine_code至少有28个元素，并且第24个位置之前的元素已经被正确设置
    // 这里使用右移和与操作来提取消息大小的各个字节
    machine_code[24] = (message_size & 0xFF) >> 0;
    // 提取message_size的最低字节。0xFF是一个十六进制数，其二进制表示为11111111。
    // 与message_size进行与操作后，保留了message_size的最低8位（一个字节）。
    // 然后，通过右移0位（实际上没有移动），将结果存储到machine_code[24]。

    machine_code[25] = (message_size & 0xFF00) >> 8;
    // 提取message_size的次低字节。0xFF00是一个十六进制数，其二进制表示为1111111100000000。
    // 与message_size进行与操作后，保留了message_size的第二个8位（即次低字节）。
    // 然后，通过右移8位，将结果存储到machine_code[25]。

    machine_code[26] = (message_size & 0xFF0000) >> 16;
    // 提取message_size的次高字节。保留了第三个8位，并通过右移16位将其存储到machine_code[26]。

    machine_code[27] = (message_size & 0xFF000000) >> 24;
    // 提取message_size的最高字节。保留了最高的8位，并通过右移24位将其存储到machine_code[27]。
}

// 打印机器码向量的内容
void show_machine_code(const std::vector<uint8_t> &machine_code)
{
    int konto = 0; // 计数器，用于控制输出格式
    std::cout << "\nMachine code generated:\n";
    std::cout << std::hex; // 设置输出格式为十六进制
    for (auto e : machine_code)
    {
        std::cout << (int)e << " "; // 输出机器码向量的每个元素，转换为int类型后输出
        konto++;
        if (konto % 7 == 0)
        { // 每输出7个元素后换行，使输出格式整齐
            std::cout << '\n';
        }
    }
    std::cout << std::dec; // 恢复输出格式为十进制
    std::cout << "\n\n";   // 输出两个换行符，使输出更易读
}
