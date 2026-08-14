#include "debug.h"
#include "utils.h"
#include <common.h>
#include <elf.h>
#include <stddef.h>
#include <utils/ftrace.h>

typedef struct {
    vaddr_t start;
    uint32_t size;
    const char *name;
} Function;

//保存所有函数信息
static Function *functions = NULL;
//函数数量
static size_t nr_functions = 0;

//保存整个ELF字符串表
static char *string_table = NULL;


#define FTRACE_STACK_SIZE 1024
typedef struct {
    const char *name;
    vaddr_t target;
    vaddr_t return_addr;
} FtraceFrame;


static FtraceFrame call_stack[FTRACE_STACK_SIZE];
static size_t call_depth = 0;


static void read_at(FILE *fp, long offset, void *buf, size_t size) {
    //1.把文件指针直接设置到距离文件开头 offset 字节的位置。
    int ret = fseek(fp, offset, SEEK_SET);
    Assert(ret == 0, "ftrace: failed to seek to file offset 0x%lx", offset);

    //2.读取
    size_t nread = fread(buf, 1, size, fp);
    Assert(nread == size, "ftrace: expected to read %zu bytes, but read %zu bytes", size, nread);
    

}

//判断一个符号是否是当前ELF中定义的函数
static bool is_valid_function(const Elf32_Sym *sym, size_t strtab_size) {
    if(ELF32_ST_TYPE(sym->st_info) != STT_FUNC) {
        return false;
    }
    //SHN_UNDEF表示该符号只被引用，没有在当前ELF中定义
    if(sym->st_shndx == SHN_UNDEF) {
        return false;
    }
    //防止st_name 越过字符串表
    if(sym->st_name >= strtab_size) {
        return false;
    }
    if(string_table[sym->st_name] == '\0') {
        return false;
    }
    return true;
}


void init_ftrace(const char *elf_file) {
    if(elf_file == NULL) {
        Log("ftrace: no ELF is given");
        return;
    }
    //1.打开文件
    FILE *fp = fopen(elf_file, "rb");
    Assert(fp != NULL, "ftrace: cannot open ELF file '%s'", elf_file);

    //2.读取ELF header
    Elf32_Ehdr ehdr;
    read_at(fp, 0, &ehdr, sizeof(ehdr));

    //3.检查ELF魔数
    Assert(memcmp(ehdr.e_ident, ELFMAG, SELFMAG) == 0, "ftrace: '%s' is not an ELF file", elf_file);

    /*
    * 当前实现只处理 RV32 对应的 ELF32。
    */
    Assert(
        ehdr.e_ident[EI_CLASS] == ELFCLASS32,
        "ftrace: '%s' is not an ELF32 file",
        elf_file
    );

    /*
    * 当前代码直接把 ELF 字节读进结构体，
    * 因此要求目标 ELF 是小端格式。
    */
    Assert(
        ehdr.e_ident[EI_DATA] == ELFDATA2LSB,
        "ftrace: only little-endian ELF is supported"
    );

    /*
    * 检查目标架构是否为 RISC-V。
    */
    Assert(
        ehdr.e_machine == EM_RISCV,
        "ftrace: '%s' is not a RISC-V ELF file",
        elf_file
    );

    /*
        4.根据ELF header 找到 Section Header Table（节头表）

        e_shoff: Setction Header Table 的文件偏移
        e_shnum: Section Header 的数量
        e_shentsize: 每个Setction Header的大小
    */
    Assert(ehdr.e_shentsize == sizeof(Elf32_Shdr),
       "ftrace: unexpected section-header size: %u",
       ehdr.e_shentsize 
    );

    size_t shdr_table_size = (size_t)ehdr.e_shnum * sizeof(Elf32_Shdr);

    Elf32_Shdr *shdrs = malloc(shdr_table_size);
    Assert(shdrs != NULL, "ftrace: failed to allocate section_header table");

    read_at(fp, ehdr.e_shoff, shdrs, shdr_table_size);
    
    /*
     *   5.遍历Section Header Table，找到符号表
     *   直接查找sh_type == SHT_SYMTAB即可
     */
    Elf32_Shdr *symtab_shdr = NULL;

    for(size_t i = 0; i < ehdr.e_shnum; i ++) {
        if(shdrs[i].sh_type == SHT_SYMTAB) {
            symtab_shdr = &shdrs[i];
            break;
        }
    }
    Assert(symtab_shdr != NULL, "ftrace: no symbol table is found in '%s'", elf_file);

    // 6.通过符号表的sh_link找到字符串表

    // 检查：符号表引用的字符串表的节下标，是否在节头表数组的合法范围内，有没有数组越界。
    Assert(symtab_shdr->sh_link < ehdr.e_shnum,
            "ftrace: invalid string-table section index");
    
    //字符串表
    //从节头表数组中，取出字符串表对应的节头指针
    Elf32_Shdr *strtab_shdr = &shdrs[symtab_shdr->sh_link];
    Assert(
      strtab_shdr->sh_type == SHT_STRTAB,
      "ftrace: symbol table does not link to a string table"
    );

    // 7.读取字符串表（字符串表必须一直保留，因为后面保存的函数名会直接指向string_table内部）
    string_table = malloc(strtab_shdr->sh_size);
    Assert(string_table != NULL, "ftrace: failed to allocate string table");

    //strtab_shdr->sh_offset:这个节的实际内容，在 ELF 磁盘文件中的起始字节偏移量（相对于文件开头的字节数）
    read_at(fp, strtab_shdr->sh_offset, string_table, strtab_shdr->sh_size);


    // 8.读取符号表
    Assert(symtab_shdr->sh_entsize == sizeof(Elf32_Sym),
            "ftrace: unexpected symbol-table entry size : %u",
            symtab_shdr->sh_entsize);

    size_t nr_symbols = symtab_shdr->sh_size / symtab_shdr->sh_entsize;

    Elf32_Sym *symbols = malloc(symtab_shdr->sh_size);
    Assert(symbols != NULL, "ftrace: failed to allocate symbol table");

    read_at(fp, symtab_shdr->sh_offset, symbols, symtab_shdr->sh_size);

    // 9.第一次遍历：统计有多少个有效函数
    nr_functions = 0;
    
    for(size_t i = 0; i < nr_symbols; i ++) {
        if(is_valid_function(&symbols[i], strtab_shdr->sh_size)) {
            nr_functions ++;
        }
    }

    // 10.为函数表分配内存
    if(nr_functions > 0) {
        functions = malloc(nr_functions * sizeof(Function));
    }
    Assert(functions != NULL, "ftrace:failed to allocate function table");

    // 11.第二次遍历：真正保存函数信息
    size_t index = 0;

    for(size_t i = 0; i < nr_symbols; i ++) {
        Elf32_Sym *sym = &symbols[i];

        if(!is_valid_function(sym, strtab_shdr->sh_size)) {
            continue;
        }

        functions[index].start = sym->st_value;
        functions[index].size = sym->st_size;

        functions[index].name = string_table + sym->st_name;

        index ++;
    }
    Assert(index == nr_functions, "ftrace:inconsisteng function count");
    
    // 释放symbols 和shdrs， string_table不能释放，因为函数名还指向它
    free(symbols);  //符号表项数组
    free(shdrs);  //节头表
    fclose(fp);

    Log("ftrace: loaded %zu functions from '%s'", nr_functions, elf_file);


    // /*
    // * 调试阶段先打印所有函数，
    // * 确认解析结果与 readelf -s 一致。
    // */
    // for (size_t i = 0; i < nr_functions; i++) {
    //     printf(
    //         "ftrace function: "
    //         "%-24s start = " FMT_WORD
    //         ", size = %u\n",
    //         functions[i].name,
    //         functions[i].start,
    //         functions[i].size
    //     );
    // }
}

const char *ftrace_find_function(vaddr_t addr) {
    
    // 先匹配函数入口
    for (size_t i = 0; i < nr_functions; i++) {
        if (addr == functions[i].start) {
            return functions[i].name;
        }
    }

    for(size_t i = 0; i < nr_functions; i ++) {
        vaddr_t start = functions[i].start;
        vaddr_t end = functions[i].size + start;

        if(functions[i].size != 0 && addr >= start && addr < end) {
            return functions[i].name;
        }
    }
    return NULL;
}

void ftrace_call(vaddr_t pc, vaddr_t target) {
    const char *name = ftrace_find_function(target);

    log_write(
        FMT_WORD ": %*scall [%s@" FMT_WORD "]\n",
        pc,
        (int)call_depth * 2,
        "",
        name != NULL ? name : "???",
        target
    );;

    Assert(call_depth < FTRACE_STACK_SIZE, "ftrace: call stack overflow");

    call_stack[call_depth].name = name != NULL ? name : "???";
    call_stack[call_depth].return_addr = pc + 4;
    call_stack[call_depth].target = target;

    call_depth ++;
}

void ftrace_ret(vaddr_t pc, vaddr_t target) {

    if(call_depth == 0) {
        log_write(
            FMT_WORD ": ret to " FMT_WORD " (empty ftrace stack)\n", pc, target
        );
        return;
    }

    call_depth --;

    FtraceFrame *frame = &call_stack[call_depth];

    log_write(
        FMT_WORD ": %*sret  [%s]\n",
        pc,
        (int)call_depth * 2,
        "",
        frame->name
    );


}

void ftrace_print_stack(void) {
    printf("FTRACE call stack: depth = %zu\n", call_depth);

    for(size_t i = call_depth; i > 0; i --) {
        FtraceFrame *frame = &call_stack[i - 1];

        printf(
            "  #%zu %s target=" FMT_WORD
            " return=" FMT_WORD "\n",
            call_depth - i,
            frame->name,
            frame->target,
            frame->return_addr
        );
    }
}