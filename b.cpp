#include <iostream>
#include <fstream>
#include <cstring>
using namespace std;

typedef struct elf32_hdr {
  unsigned char e_ident[16];
  uint16_t e_type;
  uint16_t e_machine;
  uint32_t e_version;
  uint32_t e_entry;
  uint32_t e_phoff;
  uint32_t e_shoff;
  uint32_t e_flags;
  uint16_t e_ehsize;
  uint16_t e_phentsize;
  uint16_t e_phnum;
  uint16_t e_shentsize;
  uint16_t e_shnum;
  uint16_t e_shstrndx;
} Elf32_Ehdr;

typedef struct elf32_shdr {
  uint32_t sh_name;
  uint32_t sh_type;
  uint32_t sh_flags;
  uint32_t sh_addr;
  uint32_t sh_offset;
  uint32_t sh_size;
  uint32_t sh_link;
  uint32_t sh_info;
  uint32_t sh_addralign;
  uint32_t sh_entsize;
} Elf32_Shdr;

typedef struct elf32_sym {
  uint32_t st_name;
  uint32_t st_value;
  uint32_t st_size;
  unsigned char st_info;
  unsigned char st_other;
  uint16_t st_shndx;
} Elf32_Sym;

int check_elf(ifstream &file) {
  char MAGIC[] = {0x7f,0x45,0x4c,0x46,1,1,1,0,0,0,0,0,0,0,0,0};
  Elf32_Ehdr header;
  file.read((char*)&header, sizeof(header));
  for (int i = 0; i < sizeof(MAGIC); i++)
    if (header.e_ident[i] != MAGIC[i]) return 0;
  return header.e_shoff + header.e_shnum * header.e_shentsize;
}

int get_rodata_index(Elf32_Shdr *header, char *base, int num, int strndx,
                     char *new_rodata) {
  char *table = &base[header[strndx].sh_offset];
  char temp[9]; temp[8] = 0;
  char *rodata;
  int i;
  for (i = 0; i < num; i++) {
    rodata = &table[header[i].sh_name];
    strncpy(temp, rodata, 8);
    if (!strcmp(temp, ".rodata.")) break;
  }
  if (i == num) return 0;
  header[strndx].sh_size += strlen(new_rodata) - strlen(rodata);
  return i;
}

void dumpStringTable(char *tab) {
  cout << tab << endl;
  while (*++tab) {
    cout << tab << endl;
    while (*++tab);
  }
}

void set_shstrtab(char *shstrtab, Elf32_Shdr *header, char *base, int num,
                  int strndx, char *rodata, int rodata_index) {
  char *table = &base[header[strndx].sh_offset];
  int name = header[rodata_index].sh_name;
  int delta = strlen(rodata) - strlen(&table[header[rodata_index].sh_name]);
  for (int i = 0; i < header[strndx].sh_size; i++) shstrtab[i] = 0;
  for (int i = 0; i < num; i++) {
    int dest = header[i].sh_name;
    if (dest > name) dest += delta;
    if (i == rodata_index) strcpy(&shstrtab[dest], rodata);
    else strcpy(&shstrtab[dest], &table[header[i].sh_name]);
    header[i].sh_name = dest;
  }
}

int set_symbol(char *filename, int size, Elf32_Shdr *header,
               char *base, int num, int strndx, int stringsize) {
  char *table = &base[header[strndx].sh_offset];
  int i, strtab_index;
  for (i = 0; i < num; i++)
    if (!strcmp(&table[header[i].sh_name], ".strtab")) break;
  if (i == num) return 0;
  strtab_index = i;
  header[i].sh_size = size;
  for (i = 0; i < num; i++)
    if (!strcmp(&table[header[i].sh_name], ".symtab")) break;
  if (i == num) return 0;
  Elf32_Sym *symtab = (Elf32_Sym*)&base[header[i].sh_offset];
  for (i = 0; i < 15; i++)
    if (symtab[i].st_name > 1) {
      symtab[i].st_name = strlen(filename) + 2;
      symtab[i].st_size = stringsize;
    }
  return strtab_index;
}

int bubble_sort(int *bubble, Elf32_Shdr *header, int num) {
  for (int i = 0; i < num; i++) bubble[i] = i;
  bool swap = true;
  while (swap) {
    swap = false;
    for (int i = 1; i < num; i++)
      if (header[bubble[i - 1]].sh_offset > header[bubble[i]].sh_offset) {
        int t = bubble[i - 1];
        bubble[i - 1] = bubble[i];
        bubble[i] = t;
        swap = true;
      }
  }
  int offset = 0; 
  for (int i = 0; i < num; i++) {
    if (!offset) offset = header[bubble[i]].sh_offset;
    uint32_t align = header[bubble[i]].sh_addralign - 1;
    while (offset & align) offset++;
//    header[bubble[i]].sh_offset = offset;
//    cout << hex << offset << endl;
    offset += header[bubble[i]].sh_size;
  }
  while (offset & 3) offset++;
  return offset;
}

void copy_section(char *dest, char *source, int *bubble, Elf32_Shdr *header,
                  int num, int strtab_index, int rodata_index, int shstrndx,
                  char *strtab, char *rodata, char *shstrtab) {
  int offset = 0;
  for (int i = 0; i < num; i++) {
    if (!offset) offset = header[bubble[i]].sh_offset;
    uint32_t align = header[bubble[i]].sh_addralign - 1;
    while (offset & align) offset++;
    if (bubble[i] == strtab_index) 
      memcpy(&dest[offset], strtab, header[strtab_index].sh_size);
    else if (bubble[i] == rodata_index)
      memcpy(&dest[offset], rodata, header[rodata_index].sh_size);
    else if (bubble[i] == shstrndx)
      memcpy(&dest[offset], shstrtab, header[shstrndx].sh_size);
    else memcpy(&dest[offset], &source[header[bubble[i]].sh_offset],
                header[bubble[i]].sh_size);
    header[bubble[i]].sh_offset = offset;
    offset += header[bubble[i]].sh_size;
  }
  while (offset & 3) offset++;
  memcpy(&dest[offset], header, sizeof(Elf32_Shdr) * num);
}

int main(int argc, char* argv[]) {
  char *name;
  Elf32_Ehdr *header;
  Elf32_Shdr *section_header;
  if (argc < 3) {
    cout << "./a.out sample.o source [name]" << endl;
    return 1;
  }
  char argv2[strlen(argv[2] + 1)];
  strcpy(argv2, argv[2]);
  for (int i = 0; i < strlen(argv2); i++)
    if (argv2[i] == '.') argv2[i] = '_';
  name = argc == 4 ? argv[3] : argv2;
  char rodata[strlen(name) + 9] = ".rodata.";
  strcpy(&rodata[8], name);
  ifstream file(argv[1], ios::binary);
  if (!file) {
    cout << "unable to open sample.o" << endl;
    return 1;
  }
  int size = check_elf(file);
  if (size) {
    char sample[size];
    file.seekg(0, ios::beg); file.read(sample, size); file.close();
    ifstream sfile(argv[2], ios::binary);
    if (!sfile) {
      cout << "unable to open source" << endl;
      return 1;
    }
    sfile.seekg(0, ios::end);
    size = sfile.tellg();
    sfile.seekg(0, ios::beg);
    char source[++size];
    sfile.read(source, size);
    sfile.close();
    source[size - 1] = 0;
    header = (Elf32_Ehdr*)sample;
    section_header = (Elf32_Shdr*)(&sample[header->e_shoff]);
    int bubble[header->e_shnum];
    int temp = strlen(argv[2]) + strlen(name) + 3;
    char symbol[temp];
    symbol[0] = 0;
    strcpy(&symbol[1], argv[2]);
    strcpy(&symbol[strlen(argv[2]) + 2], name);
    int strtab_index = set_symbol(argv[2], temp, section_header, sample,
                                  header->e_shnum, header->e_shstrndx, size);
    if (!strtab_index) {
      cout << "symbol not found" << endl;
      return 1;
    }
    int rodata_index = get_rodata_index(section_header, sample, header->e_shnum,
                                        header->e_shstrndx, rodata);
    if (!rodata_index) {
      cout << ".rodata.* not found" << endl;
      return 1;
    }
    section_header[rodata_index].sh_size = size;
    char shstrtab[section_header[header->e_shstrndx].sh_size];
    set_shstrtab(shstrtab, section_header, sample, header->e_shnum,
                 header->e_shstrndx, rodata, rodata_index);
    temp = bubble_sort(bubble, section_header, header->e_shnum);
    header->e_shoff = temp;
    size = temp + header->e_shnum * header->e_shentsize;
    char output[size];
    for (int i = 0; i < size; i++) output[i] = 0;
    memcpy(output, sample, sizeof(Elf32_Ehdr));
    copy_section(output, sample, bubble, section_header, header->e_shnum,
                 strtab_index, rodata_index, header->e_shstrndx,
                 symbol, source, shstrtab);
    char fn[strlen(name) + 3];
    strcpy(fn, name);
    strcpy(&fn[strlen(name)], ".o");
    ofstream ofile(fn, ios::binary);
    if (ofile) {
      ofile.write(output, size);
      ofile.close();
    }
  } else {
    cout << "It is not ELF file" << endl;
    file.close();
    return 1;
  }
  return 0;
}
