#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define PAGE_SIZE 4096
#define PAGE_TABLE_SIZE 1024
#define PHYSICAL_MEM_SIZE (PAGE_SIZE * PAGE_TABLE_SIZE)

uint8_t hd_mem[PHYSICAL_MEM_SIZE];
uint8_t ra_mem[PAGE_SIZE];

struct seitentabellen_zeile {
	uint8_t present_bit;
	uint8_t dirty_bit;
	int8_t page_frame;
	int8_t access_time;
} seitentabelle[PAGE_TABLE_SIZE];

uint16_t get_seiten_nr(uint32_t virt_address) {
	return virt_address >> 12;
}

uint16_t virt_2_ram_address(uint32_t virt_address) {
	uint16_t page_offset = virt_address & 0xFFF;
	uint16_t page_frame = seitentabelle[get_seiten_nr(virt_address)].page_frame;
	return (page_frame << 12) | page_offset;
}

int8_t check_present(uint32_t virt_address) {
	return seitentabelle[get_seiten_nr(virt_address)].present_bit;
}

int8_t is_mem_full() {
	for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
		if (seitentabelle[i].present_bit == 0) {
			return 0; // There is still free space in memory
		}
	}
	return 1; // Memory is full
}

int8_t write_page_to_hd(uint32_t seitennummer, uint32_t virt_address) {
	if (seitentabelle[seitennummer].dirty_bit) {
		// Write the page back to hd_mem
		uint16_t page_frame = seitentabelle[seitennummer].page_frame;
		for (int i = 0; i < PAGE_SIZE; i++) {
			hd_mem[page_frame * PAGE_SIZE + i] = ra_mem[i];
		}
		return 1; // Page successfully written to disk
	}
	return 0; // Page was not dirty, no need to write back to disk
}

uint16_t swap_page(uint32_t virt_address) {
	// Select a page to swap out (LRU strategy)
	// For simplicity, we just select the first non-present page
	int8_t victim_page = -1;
	for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
		if (seitentabelle[i].present_bit == 0) {
			victim_page = i;
			break;
		}
	}

	// If all pages are present, select the least recently used page (LRU)
	if (victim_page == -1) {
		for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
			if (seitentabelle[i].present_bit && (victim_page == -1 || seitentabelle[i].access_time < seitentabelle[victim_page].access_time)) {
				victim_page = i;
			}
		}
	}

	if (write_page_to_hd(victim_page, virt_address)) {
		// If the victim page was dirty, it was written back to hd_mem.
		// So we need to reset the dirty bit and remove the page from RAM.
		seitentabelle[victim_page].dirty_bit = 0;
		seitentabelle[victim_page].present_bit = 0;
		seitentabelle[victim_page].page_frame = -1;
	}

	return victim_page;
}

int8_t get_page_from_hd(uint32_t virt_address) {
	if (is_mem_full()) {
		uint16_t victim_page = swap_page(virt_address);
		// If there is no free page in memory, we need to swap one out (LRU strategy)
		if (seitentabelle[victim_page].dirty_bit) {
			// Write the page back to hd_mem
			write_page_to_hd(victim_page, virt_address);
		}
	}

	// Load the page from hd_mem to ra_mem
	uint16_t page_frame = get_seiten_nr(virt_address);
	for (int i = 0; i < PAGE_SIZE; i++) {
		ra_mem[i] = hd_mem[page_frame * PAGE_SIZE + i];
	}
	seitentabelle[page_frame].page_frame = page_frame;
	seitentabelle[page_frame].present_bit = 1;
	seitentabelle[page_frame].dirty_bit = 0;
	seitentabelle[page_frame].access_time = time(NULL);

	return 1;
}

uint8_t get_data(uint32_t virt_address) {
	uint16_t page_frame = get_seiten_nr(virt_address);
	if (!seitentabelle[page_frame].present_bit) {
		get_page_from_hd(virt_address);
	}

	return ra_mem[virt_address & 0xFFF];
}

void set_data(uint32_t virt_address, uint8_t value) {
	uint16_t page_frame = get_seiten_nr(virt_address);
	if (!seitentabelle[page_frame].present_bit) {
		get_page_from_hd(virt_address);
	}

	ra_mem[virt_address & 0xFFF] = value;
	seitentabelle[page_frame].dirty_bit = 1;
	seitentabelle[page_frame].access_time = time(NULL);
}

int main(void) {
		puts("test driver_");
	static uint8_t hd_mem_expected[4194304];
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


	uint32_t zufallsadresse = 4192425;
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

	for (uint32_t i = 0; i <= 1000;i++) {
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

	for (uint32_t i = 0; i <= 100;i++) {
		uint32_t zufallsadresse = rand() % 4095 *7;
		uint8_t value = (uint8_t)zufallsadresse >> 1;
		set_data(zufallsadresse, value);
		hd_mem_expected[zufallsadresse] = value;
//		printf("i : %d set_data address: %d - %d value at ram: %d\n",i,zufallsadresse,(uint8_t)value, ra_mem[virt_2_ram_address(zufallsadresse)]);
	}



	srand(4);
	for (uint32_t i = 0; i <= 16;i++) {
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
	for (uint32_t i = 0; i <= 2500;i++) {
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

	puts("test end");
	fflush(stdout);
	return EXIT_SUCCESS;
}