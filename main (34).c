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

uint16_t get_seiten_nr(uint32_t virt_address) {
	/**
	 *
	 */
	return virt_address >> 12;
}

uint16_t virt_2_ram_address(uint32_t virt_address) {
	/**
	 * Wandelt eine virtuelle Adresse in eine physikalische Adresse um.
	 * Der Rückgabewert ist die physikalische 16 Bit Adresse.
	 */

	uint16_t seiten_nr = get_seiten_nr(virt_address);
    uint16_t offset = virt_address & 0xFFF;
    return (seitentabelle[seiten_nr].page_frame << 12) | offset;
}

int8_t check_present(uint32_t virt_address) {
	/**
	 * Wenn eine Seite im Arbeitsspeicher ist, gibt die Funktion "check_present" 1 zurück, sonst 0
	 */
	return seitentabelle[get_seiten_nr(virt_address)].present_bit;
}

int8_t is_mem_full() {
	/**
	 * Wenn der Speicher voll ist, gibt die Funktion 1 zurück;
	 */
	for (int i = 0; i < 1024; i++) {
        if (seitentabelle[i].present_bit == 0) {
            return 0;
        }
    }
    return 1;
}

int8_t write_page_to_hd(uint32_t seitennummer, uint32_t virt_address) { // alte addresse! nicht die neue!
	/**
	 * Schreibt eine Seite zurück auf die HD
	 */

	if (seitentabelle[seitennummer].dirty_bit == 1) {
        for (int i = 0; i < 4096; i++) {
            hd_mem[(seitennummer << 12) + i] = ra_mem[(seitentabelle[seitennummer].page_frame << 12) + i];
        }
    }
    seitentabelle[seitennummer].dirty_bit = 0;
    return 1;
}

// Eine Bitmap zur Verfolgung der verwendeten physischen Rahmen
uint8_t frame_bitmap[64];  // 64 * 8 = 512. Wir haben 1024 physische Rahmen, also reicht dies aus

int8_t get_free_frame() {
    for (int i = 0; i < 64; i++) {
        for (int j = 0; j < 8; j++) {
            if (!(frame_bitmap[i] & (1 << j))) {
                frame_bitmap[i] |= (1 << j);  // Markieren Sie den Rahmen als verwendet
                return (i * 8) + j;
            }
        }
    }
    return -1;  // Keine freien Frames verfügbar
}

void free_frame(int8_t frame) {
    int byte_index = frame / 8;
    int bit_index = frame % 8;
    frame_bitmap[byte_index] &= ~(1 << bit_index);  // Markieren Sie den Rahmen als nicht verwendet
}

uint16_t swap_page(uint32_t virt_address) {
    int8_t swap_frame = -1;
    
    for (int i = 0; i < 1024; i++) {
        if (seitentabelle[i].present_bit == 1) {
            swap_frame = seitentabelle[i].page_frame;
            write_page_to_hd(i, i << 12);
            seitentabelle[i].present_bit = 0;
            free_frame(swap_frame);
            break;
        }
    }
    
    return swap_frame;
}

int8_t get_page_from_hd(uint32_t virt_address) {
    uint32_t seitennummer = get_seiten_nr(virt_address);
    
    int8_t frame;
    if (is_mem_full()) {
        frame = swap_page(virt_address);
    } else {
        frame = get_free_frame();
    }

    if (frame == -1) return 0; // Fehler

    for (int i = 0; i < 4096; i++) {
        ra_mem[(frame << 12) + i] = hd_mem[(seitennummer << 12) + i];
    }
    seitentabelle[seitennummer].page_frame = frame;
    seitentabelle[seitennummer].present_bit = 1;
    seitentabelle[seitennummer].dirty_bit = 0;

    return 1;
}

uint8_t get_data(uint32_t virt_address) {
	/**
	 * Gibt ein Byte aus dem Arbeitsspeicher zurück.
	 * Wenn die Seite nicht in dem Arbeitsspeicher vorhanden ist,
	 * muss erst "get_page_from_hd(virt_address)" aufgerufen werden. Ein direkter Zugriff auf hd_mem[virt_address] ist VERBOTEN!
	 * Die definition dieser Funktion darf nicht geaendert werden. Namen, Parameter und Rückgabewert muss beibehalten werden!
	 */

	if (!check_present(virt_address)) {
        get_page_from_hd(virt_address);
    }
    uint16_t ram_address = virt_2_ram_address(virt_address);
    return ra_mem[ram_address];
}

void set_data(uint32_t virt_address, uint8_t value) {
	/**
	 * Schreibt ein Byte in den Arbeitsspeicher zurück.
	 * Wenn die Seite nicht in dem Arbeitsspeicher vorhanden ist,
	 * muss erst "get_page_from_hd(virt_address)" aufgerufen werden. Ein direkter Zugriff auf hd_mem[virt_address] ist VERBOTEN!
	 */

	if (!check_present(virt_address)) {
        get_page_from_hd(virt_address);
    }
    uint16_t ram_address = virt_2_ram_address(virt_address);
    ra_mem[ram_address] = value;
    uint32_t seitennummer = get_seiten_nr(virt_address);
    seitentabelle[seitennummer].dirty_bit = 1;
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
			printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);
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