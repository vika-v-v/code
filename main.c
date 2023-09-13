#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

uint8_t hd_mem[4194304];
uint8_t ra_mem[65536];

struct pageframetabelle_zeile {
    uint8_t belegt_bit;
    uint16_t index;
    uint32_t virt_adress;
} RAM_pages[16];

struct seitentabellen_zeile {
    uint8_t present_bit;
    uint8_t dirty_bit;
    int8_t page_frame;
} seitentabelle[1024];

uint16_t get_seiten_nr(uint32_t virt_address) {
    return virt_address >> 12;
}

uint16_t virt_2_ram_address(uint32_t virt_address) {
    uint16_t ram_address;
    uint8_t page_frame = seitentabelle[get_seiten_nr(virt_address)].page_frame;
    ram_address = page_frame << 12;
    int offset = virt_address & 4095;
    ram_address = ram_address | offset;
    return ram_address;
}

int8_t check_present(uint32_t virt_address) {
    return seitentabelle[get_seiten_nr(virt_address)].present_bit;
}

int8_t mem_is_full() {
    int geladene_Seiten = 0;

    for (uint32_t i = 0; i < 1024; i++) {
        if (seitentabelle[i].present_bit == 1) {
            geladene_Seiten++;
        }
    }

    if (geladene_Seiten == 16) {
        return 1;
    } else return 0;
}

int8_t write_page_to_hd(uint32_t seitennummer, uint32_t virt_address) {
    int physik_anfang = 4096 * seitennummer;
    int physik_ende = 4096 * (seitennummer + 1);
    int32_t counting_virt_address = get_seiten_nr(virt_address) << 12;

    for (int physik_byte = physik_anfang; physik_byte < physik_ende; physik_byte++) {
        hd_mem[counting_virt_address] = ra_mem[physik_byte];
        counting_virt_address++;
    }

    return 1;
}

uint16_t swap_page(uint32_t virt_address) {
    int rand_page = rand() % 16;
    int index = RAM_pages[rand_page].index;
    int alte_adresse = RAM_pages[rand_page].virt_adress;

    if (seitentabelle[get_seiten_nr(alte_adresse)].dirty_bit == 1) {
        if (!write_page_to_hd(rand_page, alte_adresse)) {
            printf("ERROR: failed to write back to HD");
        }
    }

    int32_t counter_virt_address = get_seiten_nr(virt_address) << 12;
    int physik_anfang = 4096 * rand_page;
    int physik_ende = 4096 * (rand_page + 1);
    for (int physik_byte = physik_anfang; physik_byte < physik_ende; physik_byte++) {
        ra_mem[physik_byte] = hd_mem[counter_virt_address];
        counter_virt_address++;
    }

    seitentabelle[get_seiten_nr(alte_adresse)].present_bit = 0;

    RAM_pages[rand_page].virt_adress = virt_address;
    RAM_pages[rand_page].index = get_seiten_nr(virt_address);
    RAM_pages[rand_page].belegt_bit = 1;

    return rand_page;
}

int8_t get_page_from_hd(uint32_t virt_address) {
    int8_t page_frame;
    int32_t counter_virt_adress = get_seiten_nr(virt_address) << 12;
    
    if (!mem_is_full()) {
        for (int seitennummer = 0; seitennummer < 16; seitennummer++) {
            if (RAM_pages[seitennummer].belegt_bit == 0) {
                RAM_pages[seitennummer].index = get_seiten_nr(virt_address);
                RAM_pages[seitennummer].virt_adress = virt_address;
                RAM_pages[seitennummer].belegt_bit = 1;

                page_frame = seitennummer;

                int page_frame_anfang = 4096 * seitennummer;
                int page_frame_ende = 4096 * (seitennummer + 1);
                for (int page_frame_byte = page_frame_anfang; page_frame_byte < page_frame_ende; page_frame_byte++) {
                    ra_mem[page_frame_byte] = hd_mem[counter_virt_adress];
                    counter_virt_adress++;
                }
                break;
            }
        }
    } else {
        page_frame = swap_page(virt_address);
    }

    seitentabelle[get_seiten_nr(virt_address)].present_bit = 1;
    seitentabelle[get_seiten_nr(virt_address)].page_frame = page_frame;
    seitentabelle[get_seiten_nr(virt_address)].dirty_bit = 0;

    return 1;
}

void read_byte(uint32_t virt_address) {
    if (check_present(virt_address)) {
        uint16_t ra_address = virt_2_ram_address(virt_address);
        printf("RA: %d: %d\n", ra_address, ra_mem[ra_address]);
    } else {
        get_page_from_hd(virt_address);
        uint16_t ra_address = virt_2_ram_address(virt_address);
        printf("RA: %d: %d\n", ra_address, ra_mem[ra_address]);
    }
}

void write_byte(uint32_t virt_address, uint8_t value) {
    if (!check_present(virt_address)) {
        get_page_from_hd(virt_address);
    }
    
    uint16_t ra_address = virt_2_ram_address(virt_address);
    ra_mem[ra_address] = value;
    seitentabelle[get_seiten_nr(virt_address)].dirty_bit = 1;
}

void init_system() {
    srand((unsigned)time(NULL));
    for (int i = 0; i < 1024; i++) {
        seitentabelle[i].present_bit = 0;
        seitentabelle[i].dirty_bit = 0;
        seitentabelle[i].page_frame = -1;
    }
    for (int i = 0; i < 16; i++) {
        RAM_pages[i].belegt_bit = 0;
        RAM_pages[i].index = -1;
        RAM_pages[i].virt_adress = -1;
    }
}

int main() {
    init_system();

    for (int i = 0; i < 20; i++) {
        uint32_t addr = rand() % 4194304;
        write_byte(addr, i);
    }

    for (int i = 0; i < 20; i++) {
        uint32_t addr = rand() % 4194304;
        read_byte(addr);
    }
    
    return 0;
}
