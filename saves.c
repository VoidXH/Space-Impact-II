#include "saves.h"

/** Beolvassa a mentett szintet, ha az el lett mentve **/
void ReadSavedLevel(Uint8 *Level) {
    FILE *f = fopen("saved_level.txt", "rt"); /* Fájl megnyitása olvasásra */
    if (f) { /* Ha megnyitható a fájl, akkor próbál meg beolvasni */
        fscanf(f, "%c", Level); /* A fájlban található egyetlen bájtot olvassa be */
        fclose(f); /* Fájl bezárása */
    }
}

/** Beolvassa a mentett legjobb pontszámokat a paraméterben kapott tömbbe, ha el lettek mentve **/
void ReadTopScore(unsigned int *Arr) {
    FILE* f = fopen("top_score.txt", "rt"); /* Fájl megnyitása olvasásra */
    if (f) { /* Ha megnyitható a fájl, akkor próbál meg beolvasni */
        unsigned int* End = Arr + 10;
        while (Arr != End) /* Egyesével beolvassa a 10 értéket */
            fscanf(f, "%u", Arr++);
        fclose(f); /* Fájl bezárása */
    }
}

/** A bemenetként kapott szint számát menti el **/
void SaveLevel(Uint8 Level) {
    FILE *f = fopen("saved_level.txt", "wt"); /* Fájl megnyitása írásra */
    if (f) { /* Ha megnyitható a fájl, akkor próbál meg írni bele */
        fprintf(f, "%c", Level); /* Bájtként írja ki */
        fclose(f); /* Fájl bezárása */
    }
}

/** A bemenetként kapott 10 elemû tömbbe úgy illeszti be a második paraméterben kapott elemet, hogy az csökkenõ sorrendû maradjon, majd kiírja fájlba **/
void PlaceTopScore(unsigned int *Arr, Uint16 Entry) {
    FILE* f = fopen("top_score.txt", "wt"); /* Fájl megnyitása írásra */
    unsigned int *Start = Arr, *End = Arr + 10;
    while (Arr != End) { /* A tömb elsõ olyan eleméntek keresése, ami az újnál kisebb */
        if (*Arr < Entry) {
            int j;
            for (j = 9; j >= Arr - Start; j--) /* A tömb hátralévõ részének továbbléptetése */
                Start[j] = Start[j - 1];
            *Arr = Entry; /* Az így keletkezett helyre mehet az új érték */
            Arr = End; /* Kilépés a ciklusból */
        } else
            ++Arr; /* Ha még nincs elég hátul az új pont beszúrásához, menjen tovább */
    }
    /* Mentés */
    if (f) { /* Ha megnyílt a fájl, akkor próbál írni bele */
        for (Arr = Start; Arr != End; ++Arr) /* Egyesével kiírja a 10 értéket */
            fprintf(f, "%u ", *Arr);
        fclose(f); /* Fájl bezárása */
    }
}

/** Fájlnév hozzáfűzése egy elérési úthoz azonosító alapján **/
void FillFileName(char* Path, Uint16 FileID) {
    char Number[6]; /* Szövegként az azonosító, 100000 csak nem lesz */
    itoa(FileID, Number, 10); /* Számból szöveg készítése */
    strcat(Path, Number); /* Fájlnév hozzáfűzése az elérési úthoz */
    strcat(Path, ".dat"); /* Kiterjesztés hozzáfűzése az elérési úthoz */
}
