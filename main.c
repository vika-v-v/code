#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>


uint8_t hd_mem[4194304];
uint8_t ra_mem[65536];

struct pageframetabelle_zeile {
    uint8_t belegt_bit; //0 = nicht belegt; 1 = belegt
    uint16_t index; //damit ich um den zu einer Seite gehÃ¶rigen Index zu finden nicht durch die ganze Seitentabelle gehen muss
    uint32_t virt_adress;
} RAM_pages[16];//hier halte ich fest ob Page-Frames im RAM frei/belegt sind


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
    uint16_t ram_start_address = seitennummer << 12;
    uint32_t hd_page_start_address = virt_address << 12;
    for(uint16_t i = 0; i < 4096;i++) {
         hd_mem[(hd_page_start_address) + i] = ra_mem[ram_start_address + i];
    }


    return 0;
}



uint16_t swap_page(uint32_t virt_address) {
    /**
     * Das ist die Funktion zur Auslagerung einer Seite.
     * Wenn das "Dirty_Bit" der Seite in der Seitentabelle gesetzt ist,
     * muss die Seite zurÃ¼ck in den hd_mem geschrieben werden.
     * Welche RÃ¼ckschreibstrategie Sie implementieren mÃ¶chten, ist Ihnen Ã¼berlassen.
     * STRATEGIE: RANDOM PICK hihi <33
     */
    int rand_page = rand() % 16;
    int index = RAM_pages[rand_page].index;
    int alte_adresse = RAM_pages[rand_page].virt_adress; //adresse des blockes der RAUS-geswappt wird

    //falls der random block dirty ist, muss er erst zurÃ¼ck geschrieben werden, bevor er Ã¼berschrieben wird <3
    if (seitentabelle[get_seiten_nr(alte_adresse)].dirty_bit == 1) {
        if (!write_page_to_hd(rand_page, alte_adresse)) {
            printf("ERROR: failed to write back to HD");
        }
    }

    //schreiben von neuen AUF alten block
    int32_t counter_virt_address = get_seiten_nr(virt_address) << 12;
    int physik_anfang = 4096 * rand_page;
    int physik_ende = 4096 * (rand_page + 1);
    for (int physik_byte = physik_anfang; physik_byte < physik_ende; physik_byte++) {
        ra_mem[physik_byte] = hd_mem[counter_virt_address];
        counter_virt_address++;
    }

    seitentabelle[get_seiten_nr(alte_adresse)].present_bit = 0; //weil die Seite Ãœberschrieben wurde

    //aktualisieren der RAM_Pages Struktur
    RAM_pages[rand_page].virt_adress = virt_address;
    RAM_pages[rand_page].index = get_seiten_nr(virt_address);
    RAM_pages[rand_page].belegt_bit = 1; //ist eigentlich Ã¼berflÃ¼ssig


    return rand_page;
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

	puts("test end");
	fflush(stdout);
	return EXIT_SUCCESS;
}