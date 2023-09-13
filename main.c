#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

uint8_t hd_mem[4194304]; 
uint8_t ra_mem[65536];

struct seitentabellen_zeile {
    uint8_t present_bit;
    uint8_t dirty_bit;
    int8_t page_frame;
} seitentabelle[1024];

int32_t find_free_ram_spot() {
    for (int i = 0; i < sizeof(ra_mem); i++) {
        if (ra_mem[i] == 0) {
            return i;
        }
    }
    return -1;
}


int8_t find_empty_frame() {
    for (int i = 0; i < 16; i++) {
        int gefunden = 1;
        for (int j = 0; j < 1024; j++) {
            if (seitentabelle[j].present_bit && seitentabelle[j].page_frame == i) {
                gefunden = 0;
                break;
            }
        }
        if (gefunden == 1) return i;
    }
    return -1;
}

uint16_t get_seiten_nr(uint32_t virt_address) {
    return virt_address >> 12;
}

uint16_t virt_2_ram_address(uint32_t virt_address) {
    uint8_t page_frame = seitentabelle[get_seiten_nr(virt_address)].page_frame;
    return page_frame << 12 | virt_address & 4095;
}

int8_t check_present(uint32_t virt_address) {
    return seitentabelle[get_seiten_nr(virt_address)].present_bit;
}

int8_t is_mem_full() {
    int geladen = 0;

    for (int i = 0; i < 1024; i++) {
        if (seitentabelle[i].present_bit == 1) geladen++;
    }

    if (geladen >= 16) return 1;
    else return 0;
}

int8_t write_page_to_hd(uint32_t seitennummer, uint32_t virt_address) {
    uint16_t ram_start_address = seitennummer << 12;
    uint32_t hd_page_start_address = virt_address << 12;
    for(uint16_t i = 0; i < 4096;i++) {
         hd_mem[(hd_page_start_address) + i] = ra_mem[ram_start_address + i];
    }


    return 0;
}

uint16_t swap_page(uint32_t virt_address) {
    int page_using = rand() % 16;

    for (int i = 0; i < 1024; i++) {
        if (seitentabelle[i].present_bit && seitentabelle[i].page_frame == page_using) {
            if (seitentabelle[i].dirty_bit == 1) {
                write_page_to_hd(page_using, i << 12);
            }
            seitentabelle[i].present_bit = 0;
            break;
        }
    }

    int32_t virt_addr = get_seiten_nr(virt_address) << 12;
    int anfang = 4096 * page_using;
    for (int i = anfang; i < anfang + 4096; i++) {
        ra_mem[i] = hd_mem[virt_addr];
        virt_addr++;
    }

    return page_using;
}

int8_t get_page_from_hd(uint32_t virt_address) {
    uint16_t seiten_nr = get_seiten_nr(virt_address);
    if (is_mem_full()) {
        seitentabelle[seiten_nr].page_frame = swap_page(virt_address);
        seitentabelle[seiten_nr].page_frame = find_free_ram_spot();
    } else {
        seitentabelle[seiten_nr].page_frame = find_free_ram_spot();
        
    }
    for (int i = 0; i < 4096; i++) {
        ra_mem[(seitentabelle[seiten_nr].page_frame << 12) + i] = hd_mem[(seiten_nr << 12) + i];
    }
    seitentabelle[seiten_nr].present_bit = 1;
    seitentabelle[seiten_nr].dirty_bit = 0;
    return 1;
}

uint8_t get_data(uint32_t virt_address) {
    if (check_present(virt_address) == 0) {
        get_page_from_hd(virt_address);
    }
    uint16_t ram_address = virt_2_ram_address(virt_address);
    return ra_mem[ram_address];
}

void set_data(uint32_t virt_address, uint8_t value) {
    if(check_present(virt_address)) {
        ra_mem[virt_2_ram_address(virt_address)] = (uint8_t)value;
        seitentabelle[get_seiten_nr(virt_address)].dirty_bit = 1;

    }
    else {
        get_page_from_hd(virt_address);
        ra_mem[virt_2_ram_address(virt_address)] = value;
        seitentabelle[get_seiten_nr(virt_address)].dirty_bit = 1;
    }
}


int main(void) {
    puts("test driver_");
    static uint8_t hd_mem_expected[4194304];
    srand(1);
    fflush(stdout);
    for (int i = 0; i < 4194304; i++) {
        uint8_t val = (uint8_t) rand();
        hd_mem[i] = val;
        hd_mem_expected[i] = val;
    }

    for (uint32_t i = 0; i < 1024; i++) {
        seitentabelle[i].dirty_bit = 0;
        seitentabelle[i].page_frame = -1;
        seitentabelle[i].present_bit = 0;
    }

    uint32_t zufallsadresse = 4192425;

    uint8_t value = get_data(zufallsadresse);
//	printf("value: %d\n", value);

    if (hd_mem[zufallsadresse] != value) {
        printf("ERROR_ at Address %d, Value %d =! %d!\n", zufallsadresse, hd_mem[zufallsadresse], value);
    }

    value = get_data(zufallsadresse);

    if (hd_mem[zufallsadresse] != value) {
        printf("ERROR_ at Address %d, Value %d =! %d!\n", zufallsadresse, hd_mem[zufallsadresse], value);

    }

//		printf("Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);


    srand(3);

    for (uint32_t i = 0; i <= 1000; i++) {
        uint32_t zufallsadresse = rand() % 4194304;//i * 4095 + 1;//rand() % 4194303
        uint8_t value = get_data(zufallsadresse);
        if (hd_mem[zufallsadresse] != value) {
            printf("ERROR_ at Address %d, Value %d =! %d!\n", zufallsadresse, hd_mem[zufallsadresse], value);
            for (uint32_t i = 0; i <= 1023; i++) {
                //printf("%d,%d-",i,seitentabelle[i].present_bit);
                if (seitentabelle[i].present_bit) {
                    printf("i: %d, seitentabelle[i].page_frame %d\n", i, seitentabelle[i].page_frame);
                    fflush(stdout);
                }
            }
            exit(1);
        }
//		printf("i: %d data @ %u: %d hd value: %d\n",i,zufallsadresse, value, hd_mem[zufallsadresse]);
        fflush(stdout);
    }

    srand(3);

    for (uint32_t i = 0; i <= 100; i++) {
        uint32_t zufallsadresse = rand() % 4095 * 7;
        uint8_t value = (uint8_t) zufallsadresse >> 1;
        set_data(zufallsadresse, value);
        hd_mem_expected[zufallsadresse] = value;
//		printf("i : %d set_data address: %d - %d value at ram: %d\n",i,zufallsadresse,(uint8_t)value, ra_mem[virt_2_ram_address(zufallsadresse)]);
    }


    srand(4);
    for (uint32_t i = 0; i <= 16; i++) {
        uint32_t zufallsadresse = rand() % 4194304;//i * 4095 + 1;//rand() % 4194303
        uint8_t value = get_data(zufallsadresse);
        if (hd_mem_expected[zufallsadresse] != value) {
//			printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);
            for (uint32_t i = 0; i <= 1023; i++) {
                //printf("%d,%d-",i,seitentabelle[i].present_bit);
                if (seitentabelle[i].present_bit) {
                    printf("i: %d, seitentabelle[i].page_frame %d\n", i, seitentabelle[i].page_frame);
                    fflush(stdout);
                }
            }

            exit(2);
        }
//		printf("i: %d data @ %u: %d hd value: %d\n",i,zufallsadresse, value, hd_mem[zufallsadresse]);
        fflush(stdout);
    }

    srand(3);
    for (uint32_t i = 0; i <= 2500; i++) {
        uint32_t zufallsadresse = rand() % (4095 * 5);//i * 4095 + 1;//rand() % 4194303
        uint8_t value = get_data(zufallsadresse);
        if (hd_mem_expected[zufallsadresse] != value) {
            printf("%d\n", i);
            printf("ERROR_ at Address %d, Value %d =! %d!\n", zufallsadresse, hd_mem_expected[zufallsadresse], value);
            for (uint32_t i = 0; i <= 1023; i++) {
                //printf("%d,%d-",i,seitentabelle[i].present_bit);
                if (seitentabelle[i].present_bit) {
                    printf("i: %d, seitentabelle[i].page_frame %d\n", i, seitentabelle[i].page_frame);
                    fflush(stdout);
                }
            }
            exit(3);
        }
//		printf("i: %d data @ %u: %d hd value: %d\n",i,zufallsadresse, value, hd_mem_expected[zufallsadresse]);
        fflush(stdout);
    }

    puts("test end");
    fflush(stdout);
    return EXIT_SUCCESS;
}