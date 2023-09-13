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

int8_t find_empty_frame() {
    for (int i = 0; i < 16; i++) {
        int8_t found = 1;
        for (int j = 0; j < 1024; j++) {
            if (seitentabelle[j].present_bit && seitentabelle[j].page_frame == i) {
                found = 0;
                break;
            }
        }
        if (found) return i;
    }
    return -1;
}

uint16_t get_seiten_nr(uint32_t virt_address) {
    return virt_address >> 12;
}

uint16_t virt_2_ram_address(uint32_t virt_address) {
    uint16_t ram_address;

    uint8_t page_frame = seitentabelle[get_seiten_nr(
            virt_address)].page_frame;
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

    for (uint32_t i = 0; i < 1024; i++) {
        if (seitentabelle[i].present_bit && seitentabelle[i].page_frame == rand_page) {
            if (seitentabelle[i].dirty_bit == 1) {
                if (!write_page_to_hd(rand_page, i << 12)) {
                    printf("ERROR: failed to write back to HD");
                }
            }
            seitentabelle[i].present_bit = 0;
            break;
        }
    }

    int32_t counter_virt_address = get_seiten_nr(virt_address) << 12;
    int physik_anfang = 4096 * rand_page;
    int physik_ende = 4096 * (rand_page + 1);
    for (int physik_byte = physik_anfang; physik_byte < physik_ende; physik_byte++) {
        ra_mem[physik_byte] = hd_mem[counter_virt_address];
        counter_virt_address++;
    }

    return rand_page;
}

int8_t get_page_from_hd(uint32_t virt_address) {
    int8_t page_frame;
    int32_t counter_virt_adress = get_seiten_nr(virt_address) << 12;
    
    int8_t empty_frame = find_empty_frame();
    if (empty_frame != -1) {
        page_frame = empty_frame;

        int page_frame_anfang = 4096 * page_frame;
        int page_frame_ende = 4096 * (page_frame + 1);
        for (int page_frame_byte = page_frame_anfang; page_frame_byte < page_frame_ende; page_frame_byte++) {
            ra_mem[page_frame_byte] = hd_mem[counter_virt_adress];
            counter_virt_adress++;
        }
    } else {
        page_frame = swap_page(virt_address);
    }

    seitentabelle[get_seiten_nr(virt_address)].present_bit = 1;
    seitentabelle[get_seiten_nr(virt_address)].page_frame = page_frame;

    return 1;
}

uint8_t get_data(uint32_t virt_address) {
    uint8_t read_data;
    if (check_present(virt_address) == 0) {
        get_page_from_hd(virt_address);
    }
    uint16_t ram_address = virt_2_ram_address(virt_address);
    read_data = ra_mem[ram_address];

    return read_data;
}

void set_data(uint32_t virt_address, uint8_t value) {
    if (!check_present(virt_address)) {
        get_page_from_hd(virt_address);
    }
    uint16_t ram_address = virt_2_ram_address(virt_address);
    ra_mem[ram_address] = value;

    seitentabelle[get_seiten_nr(virt_address)].dirty_bit = 1;
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