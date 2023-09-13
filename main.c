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

uint16_t get_seiten_nr(uint32_t virt_address);
int8_t is_mem_full(void);
int8_t write_page_to_hd(uint32_t seitennummer);
uint16_t swap_page(uint32_t virt_address);
int8_t get_page_from_hd(uint32_t virt_address);
uint16_t virt_2_ram_address(uint32_t virt_address);
int8_t check_present(uint32_t virt_address);
uint8_t get_data(uint32_t virt_address);
void set_data(uint32_t virt_address, uint8_t value);

int8_t get_free_frame_in_ram(void) {
    for (int i = 0; i < 1024; i++) {
        int isUsed = 0;
        for (int j = 0; j < 1024; j++) {
            if (seitentabelle[j].present_bit && seitentabelle[j].page_frame == i) {
                isUsed = 1;
                break;
            }
        }
        if (!isUsed) return i;
    }
    return -1; // kein freier Frame gefunden
}

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
	if (!check_present(virt_address)) {
		get_page_from_hd(virt_address);
	}
	return (seitentabelle[seiten_nr].page_frame << 12) | (virt_address & 0xFFF);

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

int8_t write_page_to_hd(uint32_t seitennummer) { // alte addresse! nicht die neue!
	/**
	 * Schreibt eine Seite zurück auf die HD
	 */

	if (seitentabelle[seitennummer].dirty_bit) {
        uint32_t hd_address = seitennummer << 12;
        uint32_t ram_address = seitentabelle[seitennummer].page_frame << 12;
        for (int i = 0; i < 4096; i++) {
            hd_mem[hd_address + i] = ra_mem[ram_address + i];
        }
        seitentabelle[seitennummer].dirty_bit = 0;
        return 1;
    }
    return 0;
}

uint16_t swap_page(uint32_t virt_address) {
	/**
	 * Das ist die Funktion zur Auslagerung einer Seite.
	 * Wenn das "Dirty_Bit" der Seite in der Seitentabelle gesetzt ist,
	 * muss die Seite zurück in den hd_mem geschrieben werden.
	 * Welche Rückschreibstrategie Sie implementieren möchten, ist Ihnen überlassen.
	 */
	uint16_t old_seiten_nr = -1;
    int8_t swap_out_frame = -1;

    for (uint16_t i = 0; i < 1024; i++) {
        old_seiten_nr = i;
        swap_out_frame = seitentabelle[i].page_frame;
        write_page_to_hd(i);
        seitentabelle[i].present_bit = 0;
        seitentabelle[i].page_frame = -1;
        break;
    }

    if (swap_out_frame == -1) {
        puts("No suitable page to swap out.");
        exit(-1);
    }

    return swap_out_frame;
}

int8_t get_page_from_hd(uint32_t virt_address) {
	/**
	 * Lädt eine Seite von der Festplatte und speichert diese Daten im ra_mem (Arbeitsspeicher).
	 * Erstellt einen Seitentabelleneintrag.
	 * Wenn der Arbeitsspeicher voll ist, muss eine Seite ausgetauscht werden.
	 */

	uint16_t seiten_nr = get_seiten_nr(virt_address);
    uint16_t frame;
    if (is_mem_full()) {
        frame = swap_page(virt_address);
    } else {
        frame = get_free_frame_in_ram();
        if (frame == -1) {
            printf("Error: No free space in RAM.\n");
            exit(-1);
        }
    }

    uint32_t hd_address = seiten_nr << 12;
    uint32_t ram_address = frame << 12;
    for (int i = 0; i < 4096; i++) {
        ra_mem[ram_address + i] = hd_mem[hd_address + i];
    }

    seitentabelle[seiten_nr].present_bit = 1;
    seitentabelle[seiten_nr].page_frame = frame;
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
	return ra_mem[virt_2_ram_address(virt_address)];
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
	ra_mem[virt_2_ram_address(virt_address)] = value;
	seitentabelle[get_seiten_nr(virt_address)].dirty_bit = 1;
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
