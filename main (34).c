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
}seitentabelle[1024]; // 4194304 >> 12 = 1024

int8_t find_free_ram_spot() {
    for (int i = 0; i < 65536/4096; i++) {
        int found = 0;
        for (int j = 0; j < 1024; j++) {
            if (seitentabelle[j].present_bit == 1 && seitentabelle[j].page_frame == i) {
                found = 1;
                break;
            }
        }
        if (!found) return i;
    }
    return -1;  // wenn voll
}

uint16_t get_seiten_nr(uint32_t virt_address) {
	/**
	 *
	 */
	return virt_address >> 12;
}



int8_t check_present(uint32_t virt_address) {
	/**
	 * Wenn eine Seite im Arbeitsspeicher ist, gibt die Funktion "check_present" 1 zurück, sonst 0
	 */
	return seitentabelle[get_seiten_nr(virt_address)].present_bit;
}

int8_t is_mem_full() {
    return find_free_ram_spot() == -1;
}

int8_t write_page_to_hd(uint32_t seitennummer, uint32_t virt_address) {
    int8_t page_frame = seitentabelle[seitennummer].page_frame;
    for (int i = 0; i < 4096; i++) {
        hd_mem[(seitennummer << 12) + i] = ra_mem[(page_frame << 12) + i];
    }
    seitentabelle[seitennummer].dirty_bit = 0;
    return 1;
}

uint16_t swap_page(uint32_t virt_address) {
    // Find the page to swap using a basic FIFO (first in, first out) strategy
    static int next_to_swap = 0;
    if (seitentabelle[next_to_swap].dirty_bit) {
        write_page_to_hd(next_to_swap, virt_address);
    }
    uint16_t freed_frame = seitentabelle[next_to_swap].page_frame;
    seitentabelle[next_to_swap].present_bit = 0;
    next_to_swap = (next_to_swap + 1) % 1024;
    return freed_frame;
}


int8_t get_page_from_hd(uint32_t virt_address) {
    uint16_t seiten_nr = get_seiten_nr(virt_address);
    if (is_mem_full()) {
        seitentabelle[seiten_nr].page_frame = swap_page(virt_address);
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

uint16_t virt_2_ram_address(uint32_t virt_address) {
    if (!check_present(virt_address)) {
        get_page_from_hd(virt_address);
    }
    uint16_t seiten_nr = get_seiten_nr(virt_address);
    return (seitentabelle[seiten_nr].page_frame << 12) | (virt_address & 0xFFF);
}

uint8_t get_data(uint32_t virt_address) {
    uint16_t ram_address = virt_2_ram_address(virt_address);
    return ra_mem[ram_address];
}

void set_data(uint32_t virt_address, uint8_t value) {
    uint16_t ram_address = virt_2_ram_address(virt_address);
    ra_mem[ram_address] = value;
    seitentabelle[get_seiten_nr(virt_address)].dirty_bit = 1;
}

static uint8_t hd_mem_expected[4194304];

// Test, um RAM vollständig zu füllen
void test_fill_ram() {
    for (uint32_t i = 0; i < 1024; i++) {  // Iterate over pages
        for (uint32_t j = 0; j < 4096; j+=4) {  // Iterate over bytes in each page
            get_data(i*4096 + j);  // Read each 4th byte of each page
        }
        if (!seitentabelle[i].present_bit) {
            printf("ERROR: Page %d not present after filling RAM.\n", i);
            exit(4);
        }
    }
}

// Testen Sie set_data auf einem größeren Bereich
void test_set_data_large() {
    srand(5);
    for (uint32_t i = 0; i < 4194304; i += 4096) {  // Jede Seite
        uint32_t address = rand() % 4096 + i;
        uint8_t value = (uint8_t)rand();
        set_data(address, value);
        hd_mem_expected[address] = value;
        if (get_data(address) != value) {
            printf("ERROR: Mismatch after setting data at address %d.\n", address);
            exit(5);
        }
    }
}

// Testen Sie den Dirty-Bit
void test_dirty_bit() {
    srand(6);
    uint32_t address = rand() % 4194304;
    uint8_t original_value = get_data(address);
    set_data(address, original_value + 1);
    if (!seitentabelle[get_seiten_nr(address)].dirty_bit) {
        printf("ERROR: Dirty bit not set after changing data.\n");
        exit(6);
    }
}

// Test writing during a page swap
void test_write_during_swap() {
    for (uint32_t i = 0; i < 1024*4096; i += 4096) {
        uint8_t value = (uint8_t) (i/4096);
        set_data(i, value);
        hd_mem_expected[i] = value;
        for (uint32_t j = 1; j < 4096; j++) {
            get_data(i + j); // Just to induce page swaps
        }
    }

    // Now verify
    for (uint32_t i = 0; i < 1024*4096; i += 4096) {
        uint8_t value = get_data(i);
        if (value != hd_mem_expected[i]) {
            printf("ERROR during swap write test at address %d. Expected %d but got %d.\n", i, hd_mem_expected[i], value);
            exit(7);
        }
    }
}

// Test to ensure the FIFO swap algorithm works correctly
void test_fifo_swap() {
    // Make sure all pages are in RAM
    for (uint32_t i = 0; i < 1024; i++) {
        get_data(i * 4096);
    }

    // Trigger a page swap
    get_data(1024 * 4096);

    // The first page should have been swapped out
    if (seitentabelle[0].present_bit) {
        printf("ERROR: FIFO page swap failed. First page was not swapped out.\n");
        exit(8);
    }
}

// Test data integrity after many page swaps
void test_many_swaps() {
    srand(7);
    for (uint32_t i = 0; i < 10000; i++) {
        uint32_t address = rand() % 4194304;
        uint8_t value = get_data(address);
        if (value != hd_mem_expected[address]) {
            printf("ERROR after many swaps at address %d. Expected %d but got %d.\n", address, hd_mem_expected[address], value);
            exit(9);
        }
    }
}

int main(void) {
		puts("test driver_");
	
	srand(1);
	fflush(stdout);
	for(int i = 0; i < 4194304; i++) {
		//printf("%d\n",i);
		uint8_t val = (uint8_t)rand();
		hd_mem[i] = val;
		hd_mem_expected[i] = val;
	}

	for (uint32_t i = 0; i < 1024;i++) {
//		printf("%d\n",i);
		seitentabelle[i].dirty_bit = 0;
		seitentabelle[i].page_frame = -1;
		seitentabelle[i].present_bit = 0;
	}


	uint32_t zufallsadresse = 4182425;
	uint8_t value = get_data(zufallsadresse);
//	printf("value: %d\n", value);

	if(hd_mem[zufallsadresse] != value) {
		printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);
	}

	value = get_data(zufallsadresse);

	if(hd_mem[zufallsadresse] != value) {
			printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);

	}

//		printf("Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);


	srand(3);

	for (uint32_t i = 0; i <= 10000;i++) {
		uint32_t zufallsadresse = rand() % 4194304;//i * 4095 + 1;//rand() % 4194303
		uint8_t value = get_data(zufallsadresse);
		if(hd_mem[zufallsadresse] != value) {
			printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);
			for (uint32_t i = 0; i <= 1023;i++) {
				//printf("%d,%d-",i,seitentabelle[i].present_bit);
				if(seitentabelle[i].present_bit) {
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

	for (uint32_t i = 0; i <= 1000;i++) {
		uint32_t zufallsadresse = rand() % 4095 *7;
		uint8_t value = (uint8_t)zufallsadresse >> 1;
		set_data(zufallsadresse, value);
		hd_mem_expected[zufallsadresse] = value;
//		printf("i : %d set_data address: %d - %d value at ram: %d\n",i,zufallsadresse,(uint8_t)value, ra_mem[virt_2_ram_address(zufallsadresse)]);
	}



	srand(4);
	for (uint32_t i = 0; i <= 160;i++) {
		uint32_t zufallsadresse = rand() % 4194304;//i * 4095 + 1;//rand() % 4194303
		uint8_t value = get_data(zufallsadresse);
		if(hd_mem_expected[zufallsadresse] != value) {
//			printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);
			for (uint32_t i = 0; i <= 1023;i++) {
				//printf("%d,%d-",i,seitentabelle[i].present_bit);
				if(seitentabelle[i].present_bit) {
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
	for (uint32_t i = 0; i <= 25000;i++) {
		uint32_t zufallsadresse = rand() % (4095 *5);//i * 4095 + 1;//rand() % 4194303
		uint8_t value = get_data(zufallsadresse);
		if(hd_mem_expected[zufallsadresse] != value ) {
			printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem_expected[zufallsadresse], value);
			for (uint32_t i = 0; i <= 1023;i++) {
				//printf("%d,%d-",i,seitentabelle[i].present_bit);
				if(seitentabelle[i].present_bit) {
					printf("i: %d, seitentabelle[i].page_frame %d\n", i, seitentabelle[i].page_frame);
				    fflush(stdout);
				}
			}
			exit(3);
		}
//		printf("i: %d data @ %u: %d hd value: %d\n",i,zufallsadresse, value, hd_mem_expected[zufallsadresse]);
		fflush(stdout);
	}

	test_fill_ram();
    test_set_data_large();
    test_dirty_bit();
	test_write_during_swap();
	test_fifo_swap();
	test_many_swaps();

	puts("test end");
	fflush(stdout);
	return EXIT_SUCCESS;
}