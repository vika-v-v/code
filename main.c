#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
//#include <sys/random.h>

//Final Version
uint8_t hd_mem[4194304]; //Hard-Disk Mem., Adressen sind 22 Bit lang dh 2^22 versch. = 4mio Adressen; kompletter virt. Adressraum wird nicht genutzt
uint8_t ra_mem[65536]; //Random-Access Mem., 2^16, weil physik. Adressen 16 Bit lang sind, 4 Bit Seitenzahl + 12 Bit Offset

//geht eigentlich auch ohne
struct pageframetabelle_zeile {
    uint8_t belegt_bit; //0 = nicht belegt; 1 = belegt
    uint16_t index; //damit ich um den zu einer Seite gehÃ¶rigen Index zu finden nicht durch die ganze Seitentabelle gehen muss
    uint32_t virt_adress;
} RAM_pages[16];//hier halte ich fest ob Page-Frames im RAM frei/belegt sind

struct seitentabellen_zeile {
    uint8_t present_bit;
    uint8_t dirty_bit;
    int8_t page_frame;
} seitentabelle[1024]; // 4194304 >> 12 = 1024 (=2^10); 12 Bit Offset werden mit der >> op entfernt


uint16_t get_seiten_nr(uint32_t virt_address) {
    /**
     *
     */
    return virt_address >> 12;
}

uint16_t virt_2_ram_address(uint32_t virt_address) {
    /**
     * Wandelt eine virtuelle Adresse in eine physikalische Adresse um.
     * Der RÃ¼ckgabewert ist die physikalische 16 Bit Adresse.
     */

    uint16_t ram_address; //die 16 Bit physikalische Adress die zurÃ¼ckgegeben werden soll

    uint8_t page_frame = seitentabelle[get_seiten_nr(
            virt_address)].page_frame; //wir holen die Zahl des entsprechenden physikalischen Page Frames
    ram_address = page_frame << 12; //Page-Frame um den Offset(12 Bit) an die 4 most-significant Bits verschieben
    int offset = virt_address & 4095; //den Offset mit Bitwise-AND isolieren (4095: 12 Bits auf 1 gesetzt)
    ram_address = ram_address | offset; //den Offset mit Bitwise-OR anfÃ¼gen (die letzten 12 Bit waren ja Nullen)

    return ram_address;

}

int8_t check_present(uint32_t virt_address) {
    /**
     * Wenn eine Seite im Arbeitsspeicher ist, gibt die Funktion "check_present" 1 zurÃ¼ck, sonst 0
     * hier handelt es sich um virtuele seiten, und die Frage ob sie (an einer bestimmten physik. Seite) geladen sind...
     */
    return seitentabelle[get_seiten_nr(virt_address)].present_bit;
}

int8_t mem_is_full() {
    /**
     * Wenn der Speicher voll ist, gibt die Funktion 1 zurÃ¼ck;
     */
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
    /**
     * Schreibt eine Seite zurÃ¼ck auf die HD
     * ANNAHME: DIE SEITENNUMMER STEHT FÃœR DIE ZAHL DES PAGE FRAMES DEN WiR ZURÃœCK SCHREIBEN
     * DIE VIRT_ADRESSE IST DER ANFANG DER SEITE AUF DER HD
     */

    int physik_anfang = 4096 * seitennummer; //Anfang der Page im RAM
    int physik_ende = 4096 * (seitennummer + 1);
    int32_t counting_virt_address = get_seiten_nr(virt_address) << 12;

    for (int physik_byte = physik_anfang; physik_byte < physik_ende; physik_byte++) {
        hd_mem[counting_virt_address] = ra_mem[physik_byte];
        counting_virt_address++;
    }

    return 1;
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
    /**
     * LÃ¤dt eine Seite von der Festplatte und speichert diese Daten im ra_mem (Arbeitsspeicher).
     * Erstellt einen Seitentabelleneintrag.
     * Wenn der Arbeitsspeicher voll ist, muss eine Seite ausgetauscht werden.
     */
    int8_t page_frame;
    int32_t counter_virt_adress = get_seiten_nr(virt_address) << 12;
    //IF-ELSE FÃœR "WENN RAM VOLL IST"
    if (!mem_is_full()) {
        //erst einen page frame ermitteln
        for (int seitennummer = 0; seitennummer < 16; seitennummer++) {
            if (RAM_pages[seitennummer].belegt_bit == 0) {
                //RAM_Pages Struktur aktualisieren
                RAM_pages[seitennummer].index = get_seiten_nr(
                        virt_address); //Index festhalten, zugehÃ¶rigen Seitentabelleneintrag
                RAM_pages[seitennummer].virt_adress = virt_address;
                RAM_pages[seitennummer].belegt_bit = 1; //festhalten, dass die seite jetzt nicht mehr leer ist

                page_frame = seitennummer;

                //dann die Daten da rein schreiben
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

    //ERSTELLEN DES SEITENTABELLENEINTRAGS
    seitentabelle[get_seiten_nr(virt_address)].present_bit = 1;
    seitentabelle[get_seiten_nr(virt_address)].page_frame = page_frame;


    return 1;
    //TODO
}

uint8_t get_data(uint32_t virt_address) {
    /**
     * Gibt ein Byte aus dem Arbeitsspeicher zurÃ¼ck.
     * Wenn die Seite nicht in dem Arbeitsspeicher vorhanden ist,
     * muss erst "get_page_from_hd(virt_address)" aufgerufen werden. Ein direkter Zugriff auf hd_mem[virt_address] ist VERBOTEN!
     * Die definition dieser Funktion darf nicht geaendert werden. Namen, Parameter und RÃ¼ckgabewert muss beibehalten werden!
     */
    uint8_t read_data;
    if (check_present(virt_address) == 0) {
        get_page_from_hd(virt_address);
        //printf("%d DIREKT NACH GETPAGEFROMHD\n", seitentabelle[get_seiten_nr(virt_address)].present_bit);
    }
    uint16_t ram_address = virt_2_ram_address(virt_address);
    read_data = ra_mem[ram_address];

    return read_data;
}

void set_data(uint32_t virt_address, uint8_t value) {
    /**
     * Schreibt ein Byte in den Arbeitsspeicher zurÃ¼ck.
     * Wenn die Seite nicht in dem Arbeitsspeicher vorhanden ist,
     * muss erst "get_page_from_hd(virt_address)" aufgerufen werden. Ein direkter Zugriff auf hd_mem[virt_address] ist VERBOTEN!
     */
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
        //printf("%d\n",i);
        uint8_t val = (uint8_t) rand();
        hd_mem[i] = val;
        hd_mem_expected[i] = val;
    }

    for (uint32_t i = 0; i < 1024; i++) {
//		printf("%d\n",i);
        seitentabelle[i].dirty_bit = 0;
        seitentabelle[i].page_frame = -1;
        seitentabelle[i].present_bit = 0;
    }

    //RAM-Pages Tabelle zurÃ¼cksetzten
    for (int i = 0; i < 16; i++) {
        RAM_pages[i].belegt_bit = 0;
        RAM_pages[i].index = 0;
        RAM_pages[i].virt_adress = 0;
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