#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define PAGE_SIZE 4096
#define PAGE_TABLE_SIZE 1024
#define PHYSICAL_MEM_SIZE (PAGE_SIZE * PAGE_TABLE_SIZE)
#define UINT16_MAX_VAL 65535

typedef struct {
    uint8_t present_bit;
    uint8_t dirty_bit;
    int16_t page_frame;  // Using int16_t allows for -1 as uninitialized value
    int32_t access_time;
} PageTableEntry;

PageTableEntry page_table[PAGE_TABLE_SIZE];
uint8_t hd_mem[PHYSICAL_MEM_SIZE];
uint8_t ra_mem[PHYSICAL_MEM_SIZE];

uint16_t get_page_number(uint32_t virt_address) {
    puts("get_number");
    return virt_address >> 12;
}

uint32_t convert_to_physical_address(uint32_t virt_address) {
    puts("convert");
    uint16_t offset = virt_address & 0xFFF;
    uint16_t frame = page_table[get_page_number(virt_address)].page_frame;
    return (frame << 12) | offset;
}

int8_t is_page_present(uint32_t virt_address) {
    puts("present");
    return page_table[get_page_number(virt_address)].present_bit;
}

int8_t is_memory_full() {
    puts("mem_full");
    for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
        if (page_table[i].present_bit == 0) {
            return 0;
        }
    }
    return 1;
}

uint16_t get_free_frame() {
    puts("get_free");
    for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
        if (!page_table[i].present_bit) {
            return i;
        }
    }
    return UINT16_MAX_VAL; // Return maximum unsigned 16-bit value if no free frame found
}
void write_page_to_hd(uint16_t page_num) {
    puts("write");
    if (page_table[page_num].dirty_bit) {
        uint16_t frame = page_table[page_num].page_frame;
        for (int i = 0; i < PAGE_SIZE; i++) {
            hd_mem[page_num * PAGE_SIZE + i] = ra_mem[frame * PAGE_SIZE + i];
        }
    }
}

uint16_t evict_page() {
    puts("evict");
    int32_t oldest_time = INT32_MAX;
    int16_t evict_page_num = -1;

    // Find the least recently used page
    for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
        if (page_table[i].access_time < oldest_time && page_table[i].present_bit) {
            evict_page_num = i;
            oldest_time = page_table[i].access_time;
        }
    }

    if (evict_page_num != -1) {
        write_page_to_hd(evict_page_num);
        page_table[evict_page_num].dirty_bit = 0;
        page_table[evict_page_num].present_bit = 0;
        return page_table[evict_page_num].page_frame;
    }

    // Unexpected scenario: no page to evict
    return UINT16_MAX_VAL;
}

int8_t load_page_from_hd(uint32_t virt_address) {
    puts("load");
    uint16_t frame;

    if (is_memory_full()) {
        frame = evict_page();
    } else {
        frame = get_free_frame();
    }

    if (frame == UINT16_MAX_VAL) return 0; // Error handling

    uint16_t page_num = get_page_number(virt_address);
    for (int i = 0; i < PAGE_SIZE; i++) {
        ra_mem[frame * PAGE_SIZE + i] = hd_mem[page_num * PAGE_SIZE + i];
    }

    page_table[page_num].page_frame = frame;
    page_table[page_num].present_bit = 1;
    page_table[page_num].dirty_bit = 0;
    page_table[page_num].access_time = time(NULL);

    return 1;
}

uint8_t get_data(uint32_t virt_address) {
    puts("get_data");
    uint16_t page_num = get_page_number(virt_address);
    if (!page_table[page_num].present_bit) {
        load_page_from_hd(virt_address);
    }
    page_table[page_num].access_time = time(NULL);
    return ra_mem[convert_to_physical_address(virt_address)];
}

void set_data(uint32_t virt_address, uint8_t value) {
    puts("set_data");
    uint16_t page_num = get_page_number(virt_address);
    if (!page_table[page_num].present_bit) {
        load_page_from_hd(virt_address);
    }

    ra_mem[convert_to_physical_address(virt_address)] = value;
    page_table[page_num].dirty_bit = 1;
    page_table[page_num].access_time = time(NULL);
}

int main(void) {
    puts("start");
    srand(time(NULL));

    // Initialize hd_mem with random values.
    for (int i = 0; i < PHYSICAL_MEM_SIZE; i++) {
        hd_mem[i] = rand() % 256;
    }

    // Initialize page table.
    for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
        page_table[i].present_bit = 0;
        page_table[i].dirty_bit = 0;
        page_table[i].page_frame = -1;
        page_table[i].access_time = 0;
    }

    puts("Starting test...");

    /*
    // Create a hd_mem_expected array to hold expected results.
    uint8_t hd_mem_expected[PHYSICAL_MEM_SIZE];
    for (int i = 0; i < PHYSICAL_MEM_SIZE; i++) {
        hd_mem_expected[i] = hd_mem[i];
    }

    // Test retrieval of data from a random address.
    uint32_t random_address = 4192425;
    uint8_t value = get_data(random_address);
    if (hd_mem[random_address] != value) {
        printf("ERROR_ at Address %d, Value %d != %d!\n", random_address, hd_mem[random_address], value);
    }

    // Access a few random addresses.
    srand(3);
    for (uint32_t i = 0; i <= 1000; i++) {
        uint32_t random_address = rand() % PHYSICAL_MEM_SIZE;
        uint8_t value = get_data(random_address);
        if (hd_mem_expected[random_address] != value) {
            printf("ERROR_ at Address %d, Value %d != %d!\n", random_address, hd_mem_expected[random_address], value);
            exit(1);
        }
    }

    // Modify values at some addresses.
    srand(3);
    for (uint32_t i = 0; i <= 100; i++) {
        uint32_t random_address = (rand() % 4095) * 7;
        uint8_t new_value = (uint8_t)random_address >> 1;
        set_data(random_address, new_value);
        hd_mem_expected[random_address] = new_value;
    }

    // Validate the changes.
    srand(4);
    for (uint32_t i = 0; i <= 16; i++) {
        uint32_t random_address = rand() % PHYSICAL_MEM_SIZE;
        uint8_t value = get_data(random_address);
        if (hd_mem_expected[random_address] != value) {
            printf("ERROR_ at Address %d, Value %d != %d!\n", random_address, hd_mem_expected[random_address], value);
            exit(2);
        }
    }

    srand(3);
    for (uint32_t i = 0; i <= 2500; i++) {
        uint32_t random_address = rand() % (4095 * 5);
        uint8_t value = get_data(random_address);
        if (hd_mem_expected[random_address] != value) {
            printf("ERROR_ at Address %d, Value %d != %d!\n", random_address, hd_mem_expected[random_address], value);
            exit(3);
        }
    }
    puts("Test finished successfully.");

    return EXIT_SUCCESS;*/
}
