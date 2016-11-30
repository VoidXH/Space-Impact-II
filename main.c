#include <time.h>
#include <SDL_gfxPrimitives.h>
#include "config.h"
#include "enemies.h"
#include "graphics.h"
#include "saves.h"
#include "scenery.h"
#include "shotlist.h"
#include "audio.h"
#include "font.h"

/** Képkockafrissítõ funkció **/
Uint32 FrameUpdate(Uint32 ms, void *param) {
    SDL_Event ev;
    ev.type = SDL_USEREVENT; /* Felhasználói eseményt küld a fõprogramnak */
    if (param == NULL) /* Ez biztos NULL, mert azt adjuk át main-ben, csak a fordító figyelmeztetését tünteti el (param használatlan) */
        SDL_PushEvent(&ev);
    return ms;
}

/** Kiragadott inicializálási lépések, hogy a kód ISO C90 maradhasson **/
SDL_Surface* SDL_StartSurface(Vec2 FrameSize) { /* SDL inicializálása adott képméretre */
    SDL_Surface* s;
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER); /* Kép, hang, és időzítő is kell */
    s = SDL_SetVideoMode(FrameSize.x, FrameSize.y, 0, SDL_ANYFORMAT); /* Ablakméret átadása SDL-nek */
    if (!s) /* Ha nem sikerült megnyitni az ablakot, ne fusson tovább */
        exit(1);
    return s;
}

int main(int argc, char *argv[]) {
    /** SDL inicializálás **/
    Vec2 FrameSize = {84 * UPSCALE_FACTOR, 48 * UPSCALE_FACTOR}; /* A skálázott képkocka méretei */
    SDL_Event e;
    SDL_Surface* s = SDL_StartSurface(FrameSize);
    SDL_AudioSpec audio; /* SDL hangspecifikáció */
    Sint32 AudioFlags = 0; /* Lejátszandó hangok flag-jei */
    SDL_TimerID t; /* Képkockafrissítő időzítő */

    /** A játékhoz szükséges változók **/
    Uint8 run = 1; /* Kilépéskor hamisra vált, és a fõciklus kilép */
    int i, j, k; /* Három iterátorváltozó, késõbb szükség lesz mindre */
    Uint8 OldPixelMap[84 * 48], PixelMap[84 * 48]; /* Az elõzõ és mostani képkocka, ahol a kettõ nem egyezik, azt kell újrarajzolni */
    Uint8 IntroPhase = 12; /* A bevezető animációból hátralévő kockák */
    Uint8 FrameHold = 3; /* Ennyi képkockán át még tartsa a jelenlegit (csak a bevezető animációra érvényes) */
    PlayerObject Player; /* A játékos struktúrája */
    Sint8 Level = -1; /* Szint azonosítója, ha negatív, akkor menü */
    Uint8 MenuItem = 1; /* A menüben kiválasztott lehetõség */
    Uint8 SavedLevel = 0; /* Utoljára félbehagyott pálya */
    Uint8 PlayerUp = 0, PlayerDown = 0, PlayerLeft = 0, PlayerRight = 0, PlayerShooting = 0; /* A játékos éppen mozog vagy lõ (gombot nyom) */
    Uint8 PlayerShootTimer = 0; /* Lövés idõzítõ, 5 képkockánként indulhasson csak újabb lövés */
    Uint8 LevelCount = 0; /* Hány szintet tartalmaz a játék adatmappája? */
    FILE* LastLevel = GetLevel(0); /* Utolsó szint fájla, az utolsó szint meghatározásához szükséges */
    Shot *Shots = NULL; /* Egy láncolt lista, az összes lövést tartalmazza */
    EnemyList *Enemies = NULL; /* Ellenségek láncolt listája */
    Uint8 AnimPulse = 0; /* Animációk idõzítõje */
    Scenery *Scene = NULL; /* A táj objektumainak láncolt listája, az elsõ szinten még nincs */
    Uint8 MoveScene; /* A táj mozgatása - a szintek végén mindig megáll */
    #ifdef LEGACY_TOP_SCORE
    Uint8 TimeInScores = 0; /* A rekordképernyőn eltöltött idő, amiből az animáció képkockája számolódik */
    #endif /* LEGACY_TOP_SCORE */
    unsigned int TopScores[10]; /* A 10 legjobb pontszám, azért unsigned int, mert a Uint16-ra a %hu formátumjelző kellene, ami nem ANSI C */
    memset(TopScores, 0, sizeof(TopScores)); /* Alapértelmezetten mind 0 */
    ReadSavedLevel(&SavedLevel); /* Ha lett elmentve utolsó szint, olvassa be */
    ReadTopScore(TopScores); /* Ha vannak mentett rekordok, olvassa be őket */
    while (LastLevel) { /* Ameddig még meg lehet nyitni a növekvő szintszámmal elnevezett fájlokat, a játék addig a szintig játszható */
        fclose(LastLevel); /* Régi fájl bezárása, úgyse olvastuk, csak a létezés érdekelt */
        ++LevelCount; /* Ugrás a következő szintre */
        LastLevel = GetLevel(LevelCount); /* Próbálkozás a következő szint fájlának megnyitásával */
    }

    /** SDL játékkezelés inicializálása **/
    SDL_WM_SetCaption("Space Impact II", "Space Impact II"); /* Ablak neve */
    t = SDL_AddTimer(1000 / FRAMERATE, FrameUpdate, NULL); /* Képfrissítés időzítése */

    /** Pixeltérképek inicializálása **/
    /* A kirajzolási idõket optimalizálja, ha az SDL csak a változott pixeleket kapja meg */
    for (i = 0; i < 84 * 48; ++i)
        OldPixelMap[i] = 1; /* A régi pixeltérkép legyen teljesen aktív (=fekete), hogy a háttérrel töltse ki az első képkocka */

    /** Hang inicializálása **/
    audio.freq = SAMPLE_RATE; /* Mintavételezés az alapértelmezett frekvencián */
    audio.format = AUDIO_S16; /* Előjeles 16 bites egész minták */
    audio.channels = 1; /* Monó hang */
    audio.samples = 1024; /* Ennyi mintára hívja meg a hangkezelőt */
    audio.callback = AudioCallback; /* Hangkezelő függvény */
    audio.userdata = &AudioFlags; /* A hangkezelő függvénynek folyamatosan átadott paraméter */
    if (SDL_OpenAudio(&audio, NULL) >= 0) /* Ha sikerül inicializálni a hangot */
        SDL_PauseAudio(0); /* Indítsa is el */

    /** Pixeltérképek kibontása **/
    UncompressFont();
    UncompressObjects();

    /** Cheat **/
    if (argc > 2) /* Amúgy csak azért létezik, mert a fordító rinyál a main paramétereinek használatlansága miatt, amik létét az SDL követeli */
        if (strcmp(argv[1], "-lvl") == 0) /* Ezen attribútum után lehet beküldeni a kívánt pálya számát */
            SavedLevel = atoi(argv[2]);

    /** Fõ loop **/
    srand(time(NULL)); /* Legyen a random bónusz tényleg random */
    while (run) {
        SDL_WaitEvent(&e);
        switch(e.type) {
        /** Billentyûleütések **/
        case SDL_KEYDOWN:
            if (IntroPhase) { /* Bevezető animáció félbeszakítása, ha megy */
                IntroPhase = 0;
                AudioFlags |= SOUND_MENUBTN; /* Bármilyen gombra adjon ki hangot, még ami nem is csinál semmit */
            /** Rekord- és játék vége képernyõ **/
            } else if (Level == -2 || Level == LevelCount) {
                if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_ESCAPE) /* Enterre vagy Esc-re visszatér a főmenübe */
                    Level = -1;
                AudioFlags |= SOUND_MENUBTN; /* Bármilyen gombra adjon ki hangot, még ami nem is csinál semmit */
            /** Fõmenü **/
            } else if (Level == -1) {
                AudioFlags |= SOUND_MENUBTN; /* Bármilyen gombra adjon ki hangot, még ami nem is csinál semmit */
                if (e.key.keysym.sym == SDLK_RETURN) { /* Enter */
                    if (SavedLevel == 0)
                        MenuItem++; /* A menü elemeit a folytatással kezdõdõ menühöz viszonyítja (új játék 1. hely helyett a 2., stb.) */
                    if (MenuItem == 3) {
                        Level = -2; /* Ez a rekordképernyõ azonosítõja */
                        #ifdef LEGACY_TOP_SCORE
                        TimeInScores = 0; /* Kezdje elölről a rekordképernyő animációját */
                        #endif /* LEGACY_TOP_SCORE */
                        if (SavedLevel == 0) /* Mivel a continue nélküli képernyõnél a menüelem azonosítója nõtt, vissza kell állítani */
                            MenuItem = 2;
                    } else { /* Valamilyen szintet betöltõ opció lett kiválasztva */
                        if (MenuItem == 1) /* Folytatás */
                            Level = SavedLevel; /* Mentett szint betöltése */
                        else /* Új játék */
                            Level = 0; /* 1. szint betöltése */
                        EmptyShotList(&Shots); /* Lövéslista ürítése (hátha maradt az előző játékból) */
                        /* Játékos alaphelyzetbe állítása (még folytatás esetén is, egyedül a pálya van mentve) */
                        Player.Lives = 3; /* 3 élet */
                        Player.Score = 0;
                        Player.Bonus = 3; /* Az új játékok mindig három rakétával indulnak */
                        Player.Weapon = Missile;
                        Player.Pos = NewVec2(3, 20); /* A játékos kezdõpozíciója */
                        Player.Protection = 50; /* Kezdeti védelem */
                        PlayerShootTimer = 0; /* Hideg fegyverrel kezdés */
                        /* Játékos akcióinak nullázása, ha esetleg a játékos nem engedte fel a gombokat az Esc megnyomása elõtt */
                        PlayerUp = PlayerDown = PlayerLeft = PlayerRight = PlayerShooting = 0;
                        LevelSpawner(&Enemies, Level); /* Szint benépesítése */
                        EmptyScenery(&Scene); /* Táj ürítése, hátha egy esetleges elõzõ szintrõl maradt benne valami */
                        MoveScene = 1; /* Tájmozgatás újraindítása */
                    }
                }
                else if (e.key.keysym.sym == SDLK_ESCAPE) /* Kilépés az Esc billentyûre is */
                    run = 0;
                else if (e.key.keysym.sym == SDLK_UP) /* Fel nyíl: elõzõ menüelem, körkörösen */
                    MenuItem = MenuItem == 1 ? (SavedLevel ? 3 : 2) : (MenuItem - 1);
                else if (e.key.keysym.sym == SDLK_DOWN) /* Le nyíl: következõ menüelem, körkörösen */
                    MenuItem = MenuItem % (SavedLevel ? 3 : 2) + 1;
            /** Játék **/
            } else {
                #ifdef GHOSTING
                if (!PlayerUp && !PlayerDown && !PlayerLeft && !PlayerRight && !PlayerShooting)
                #endif /* GHOSTING */
                switch(e.key.keysym.sym) {
                    /* Nyílgombok: játékos mozgatása */
                    case SDLK_UP: PlayerUp = 1; break;
                    case SDLK_DOWN: PlayerDown = 1; break;
                    case SDLK_LEFT: PlayerLeft = 1; break;
                    case SDLK_RIGHT: PlayerRight = 1; break;
                    case SDLK_SPACE: PlayerShooting = 1; break; /* Space: tûz */
                    case SDLK_LCTRL: /* Bal és jobb Ctrl: bónuszfegyver használata */
                    case SDLK_RCTRL:
                        if (Player.Bonus) { /* Csak ha van még bónuszból */
                            /* Bónuszlövedék a játékos orrától: fal esetén a pálya tetején kezdődjön, sugár esetén lógjon bele a játékos orrába */
                            AddShot(&Shots, NewVec2(Player.Pos.x + 9, Player.Weapon == Wall ? 5 : Player.Pos.y + 2), Player.Weapon == Beam ? 0 : 2, 1, Player.Weapon);
                            --Player.Bonus; /* Bónusz elhasználása */
                            AudioFlags |= SOUND_BONUSWPN; /* Bónuszfegyver hangjának kiadása */
                        }
                        break;
                    case SDLK_ESCAPE:
                        SavedLevel = Level; /* Szint mentése */
                        SaveLevel(Level); /* Mentse el, hogy erről a szintről lépett ki a játékos */
                        Level = -1; /* Menübe lépés */
                        MenuItem = 1; /* A menü elsõ elemre állítása */
                        break;
                    default: break;
                }
            }
            break;
        case SDL_KEYUP: /* Nyomva tartott billentyûk felengedése esetén a nyomva tartást rögzítõ változók nullázása */
            /* Ilyenek csak játék alatt vannak, de nem kell ezt ellenõrizni, mert máshol nincs hatása az értékeknek */
            /* Egyébként ez switch volt, csak attól 4 bájttal nőne a generált kód */
            if (e.key.keysym.sym == SDLK_UP)
                PlayerUp = 0;
            if (e.key.keysym.sym == SDLK_DOWN)
                PlayerDown = 0;
            if (e.key.keysym.sym == SDLK_LEFT)
                PlayerLeft = 0;
            if (e.key.keysym.sym == SDLK_RIGHT)
                PlayerRight = 0;
            if (e.key.keysym.sym == SDLK_SPACE)
                PlayerShooting = 0;
            break;
        /** Képkocka összeállítása **/
        case SDL_USEREVENT:
            memset(PixelMap, 0, sizeof(PixelMap)); /* Kép ürítése */
            /** Bevezető animáció **/
            if (IntroPhase) {
                DrawObject(PixelMap, GetObject(gSpace), NewVec2(8, 12 - IntroPhase)); /* A Space felirat ússzon be felülről */
                DrawObject(PixelMap, GetObject(gImpact), NewVec2(4, 24 + IntroPhase)); /* Az Impact felirat ússzon be alulról */
                DrawOutlinedObject(PixelMap, GetObject(gIntro), NewVec2(56 - IntroPhase * 4, 20)); /* A hajók ússzanak középen */
                if (FrameHold) { /* A képkockát tartsa, vagy lépjen újra, ha letelt az ideje */
                    --FrameHold;
                    if (!FrameHold)
                        FrameHold = --IntroPhase == 1 ? FRAMERATE : 2; /* Ha az utolsó képkocka jön, egy másodpercig maradjon */
                }
            /** Játék vége képernyõ **/
            } else if (Level == LevelCount) {
                char ScoreText[6]; /* Maximum ötjegyű lehet + lezáró karakter */
                itoa(Player.Score, ScoreText, 10); /* Pontszám szöveggé alakítása */
                DrawText(PixelMap, "Game over\nYour score:", NewVec2(1, 1), 9);
                DrawText(PixelMap, ScoreText, NewVec2(1, 19), 0);
            /** Rekodrképernyõ **/
            } else if (Level == -2) {
                #ifdef LEGACY_TOP_SCORE
                const Uint8 OneSign[24] = {0,0,1,0,0,1,1,0,1,1,1,0,0,1,1,0,0,1,1,0,1,1,1,1}; /* Egy egyes pixeltérképe */
                char ScoreText[6]; /* Maximum ötjegyű lehet + lezáró karakter */
                itoa(TopScores[0], ScoreText, 10); /* Pontszám szöveggé alakítása */
                DrawText(PixelMap, "Top score:", NewVec2(1, 1), 0);
                DrawText(PixelMap, ScoreText, NewVec2(1, 11), 0); /* A rögzített legjobb pontszám kiírása */
                for (i = 0; i < 4; ++i) { /* Az egyes oszlopai */
                    for (j = 0; j < 6; ++j) { /* Az egyes sorai */
                        /* Ki gondolta volna, hogy x == 0 || x == 1 helyett x >= 0 && x <= 1 sikeresen lespórol 300 bájtot */
                        Vec2 Pos = NewVec2(64 + i * 5, 1 + j * 4);
                        if (TimeInScores / 3 - (4 - i) - j >= 0 && TimeInScores / 3 - (4 - i) - j <= 1) /* Ez egy lefelé mozgó átlóra képlet, harmad sebességgel */
                            DrawObject(PixelMap, GetObject(TimeInScores / 3 - (4 - i) - j ? gDotFull : gDotEmpty), Pos); /* Egy pixelnyi köz legyen a pontok közt */
                        else if (TimeInScores / 3 - (4 - i) - j > 1 && OneSign[j * 4 + i]) /* Ahogy mozognak az átlók lefelé, a nyomvonaluk rajzolja ki az egyest */
                            DrawObject(PixelMap, GetObject(TimeInScores < 45 ? gDotEmpty : gDotFull), Pos); /* 45-nél színváltás */
                    }
                }
                if (++TimeInScores == 54) /* Az animáció 35 és 53 között maradjon, azaz ismételgesse a villogást */
                    TimeInScores = 35;
                #else
                /* Helyezásek kiírása */
                Vec2 Pos = {3, 3}; /* Kezdõpozíció */
                for (i = 0; i < 10; i++) {
                    if (i == 5) /* Az 5. elemnél új oszlopot kell kezdeni felül */
                        Pos = NewVec2(47, 3);
                    DrawSmallNumber(PixelMap, i + 1, i == 9 ? 2 : 1, Pos); /* Helyezés */
                    DrawObject(PixelMap, GetObject(gShot), NewVec2(Pos.x + 4, Pos.y + 2)); /* Elválasztó */
                    DrawSmallNumber(PixelMap, TopScores[i], 5, NewVec2(Pos.x + 24, Pos.y)); /* Pontszám */
                    Pos.y += 9; /* A képernyõ egyenletes elosztásához 4 pixel szükséges a számok közt, egy szám pedig 5 pixel magas, így 9-et kell ugrani */
                }
                #endif /* LEGACY_TOP_SCORE*/
            /** Fõmenü **/
            } else if (Level == -1) {
                /* A menüelem-jelzõ kezdete, ez Nokia 3310-en 8-2- */
                DrawSmallNumber(PixelMap, 8, 1, NewVec2(65, 0)); /* A számírót meghívni kevesebb bájtba kerül */
                DrawObject(PixelMap, GetObject(gShot), NewVec2(69, 2));
                DrawSmallNumber(PixelMap, 2, 1, NewVec2(73, 0));
                DrawObject(PixelMap, GetObject(gShot), NewVec2(77, 2));
                DrawSmallNumber(PixelMap, gNum0 + MenuItem, 1, NewVec2(81, 0)); /* A kiválasztott menüelem száma */
                DrawText(PixelMap, SavedLevel ? "Continue\nNew game\nTop score" : "New game\nTop score", NewVec2(1, 7), 11); /* Menüelemek kiírása, az egészet szövegként */
                InvertScreenPart(PixelMap, NewVec2(0, MenuItem * 11 - 5), NewVec2(76, MenuItem * 11 + 5)); /* A kiválasztott menüelem körül invertálja a képet */
                DrawText(PixelMap, "Select", NewVec2(24, 40), 0); /* Select felirat alulra */
                DrawScrollBar(PixelMap, (MenuItem - 1) * (SavedLevel ? 50 : 100)); /* Görgetõsáv rajzolása */
            /** Játék **/
            } else {
                /* Állapotsáv */
                Uint8 NonInverseLevel = Level < 4 || 5 < Level; /* Felül van-e a táj és alul az állapotsáv? */
                Sint16 BarTop = NonInverseLevel ? 0 : 43; /* A felső behúzása az állapotsávnak, néhány pályán alul van */
                Uint8 StartLives = Player.Lives; /* A játékos élete a képkocka feldolgozása előtt */
                #ifdef ZEROTH_LIFE /* Az életek számának jelzése nulladik élet módtól függõen */
                for (i = 0; i < Player.Lives - 1; ++i)
                #else
                for (i = 0; i < Player.Lives; ++i)
                #endif
                    DrawObject(PixelMap, GetObject(gLife), NewVec2(i * 6, BarTop)); /* Szívek rajzolása */
                DrawObject(PixelMap, GetObject((Graphics)(gLife + Player.Weapon)), NewVec2(33, BarTop)); /* Bónuszfegyver ikonjának kirajzolása, ezek a szív után helyezkednek el az enumerációban */
                DrawSmallNumber(PixelMap, Player.Bonus, 2, NewVec2(43, BarTop)); /* Hátralévõ bónuszfegyveres lövések kijelzése */
                DrawSmallNumber(PixelMap, Player.Score, 5, NewVec2(71, BarTop)); /* Pontszám kijelzése */
                /* Játéktér */
                ShotListTick(&Shots, PixelMap, &Player); /* Már a pályán lévõ lövedékek kezelése */
                /* Játékos mozgása */
                if (Enemies) { /* Csak akkor mozoghat gombokkal, ha még van ellenség */
                    if (PlayerLeft && Player.Pos.x > (Player.Protection ? 2 : 0))
                        --Player.Pos.x;
                    if (PlayerRight && Player.Pos.x < 74)
                        ++Player.Pos.x;
                    /* Az alsó és felső 5 pixelre nem mehet a játékos, ha ott a kijelző */
                    if (PlayerUp && Player.Pos.y > NonInverseLevel * 5 + (Player.Protection ? 2 : 0))
                        --Player.Pos.y;
                    if (PlayerDown && Player.Pos.y < 36 + NonInverseLevel * 5 - (Player.Protection ? 2 : 0))
                        ++Player.Pos.y;
                } else { /* Különben pálya vége animáció */
                    EmptyShotList(&Shots); /* Lövéslista ürítése */
                    if (Player.Pos.x > 84) { /* Ha az animációnak vége, a játékos kiment a pályáról, töltse be a következõ szintet */
                        Player.Pos = NewVec2(3, 20); /* Pozíció vissza alapra */
                        PlayerShootTimer = 0; /* Fegyver lehûtése */
                        PlayerUp = PlayerDown = PlayerLeft = PlayerRight = PlayerShooting = 0; /* Játékos akcióinak nullázása, ha a játékos nem engedte fel a gombokat az animáció alatt sem */
                        LevelSpawner(&Enemies, ++Level); /* Következõ szint benépesítése */
                        if (Level == LevelCount) { /* Ha a következő szint a játék vége képernyő */
                            PlaceTopScore(TopScores, Player.Score); /* A végső pontszámot helyezze el a legjobbak közt */
                            SavedLevel = 0; /* Nincs honnan folytatni egy befejezett játékot */
                            SaveLevel(SavedLevel); /* Mentés törlése */
                        }
                        else /* Ha van még pálya, ürítse a tájat, hogy legyen hova rajzolni neki */
                            EmptyScenery(&Scene);
                        MoveScene = 1; /* Tájmozgatás újraindítása */
                    } else { /* Kiúszás a képernyõrõl */
                        Sint16 OutPosition = NonInverseLevel ? 10 : 31; /* A kiúszási sor attól függ, hogy felül vagy alul van-e a táj */
                        if (Player.Pos.y < OutPosition) /* Először menjen a kiúszási sorba */
                            ++Player.Pos.y; /* Ha alatta van, ússzon fel */
                        else if (Player.Pos.y > OutPosition)
                            --Player.Pos.y; /* Ha felette van, ússzon le */
                        else /* Ha már a kiúszási sorban van, ússzon ki jobbra */
                            Player.Pos.x += 3;
                    }
                }
                /* Ha van kezdeti védelem, minden páros képkockában váltson a két animációs fázisa között */
                DrawObject(PixelMap, GetObject(Player.Protection ? G_PROTECTION_A1 + (Player.Protection / 2) % 2 : G_PLAYER),
                           Player.Protection ? NewVec2(Player.Pos.x - 2, Player.Pos.y - 2) : Player.Pos);
                if (PlayerShootTimer) /* Ha a fegyver nem hûlt ki... */
                    PlayerShootTimer--; /* ...akkor hûljön ki */
                if (PlayerShooting && PlayerShootTimer == 0) { /* Csak 0-s idõzítõvel lehet lõni */
                    AddShot(&Shots, NewVec2(Player.Pos.x + 9, Player.Pos.y + 3), 2, 1, Standard); /* Lövés elhelyezése a pályán */
                    PlayerShootTimer = 4; /* Felmelegszik a fegyver, a következõ 5 képkockában nem lehet vele lõni */
                    AudioFlags |= SOUND_SHOT; /* Lövéshang kérése a következő hangkiadáskor */
                }
                AnimPulse = 1 - AnimPulse; /* 1 és 0 között ugrál, hogy felezze az animációk sebességét, különben túl gyors */
                EnemyListTick(&Enemies, &Player, PixelMap, &Shots, AnimPulse, &MoveScene); /* Ellenségek kezelése */
                HandleScenery(&Scene, PixelMap, MoveScene, &Player, Level); /* Táj mutatása */
                if (Level == 0 || !NonInverseLevel) /* Néhány szinten invertált a kép */
                    InvertScreen(PixelMap);
                if (Player.Protection) /* Ha a játékosnak van védelme */
                    Player.Protection--; /* Fogyjon el */
                if (Player.Lives == 0) { /* A játékos halála esetén */
                    PlaceTopScore(TopScores, Player.Score); /* Pontszám elhelyetése a legjobbak közt és mentés */
                    Level = LevelCount; /* Ugrás a játék vége képernyőre, ami az utolsó pálya helyett van */
                    SavedLevel = 0; /* Nincs honnan folytatni egy befejezett játékot */
                    SaveLevel(SavedLevel); /* Mentés törlése */
                    AudioFlags |= SOUND_DEATH; /* Halálhang kiadása */
                } else if (Player.Lives != StartLives) { /* Ha a játékos a kör alatt életet veszített */
                    Player.Pos = NewVec2(3, 20); /* Kerüljön a kezdőpozícióba */
                    Player.Protection = 50; /* Kapjon védelmet */
                    AudioFlags |= SOUND_DEATH; /* Halálhang kiadása */
                }
            }
            /** Kirajzolás **/
            for (i = 0; i < 84 * 48; i++) { /* Az eredeti kép méretei alapján */
                if (PixelMap[i] != OldPixelMap[i]) { /* Csak a változásokkal írja felül a képet */
                    int x = (i % 84) * UPSCALE_FACTOR; /* Oszlop szána */
                    int y = (i / 84) * UPSCALE_FACTOR; /* Sor száma */
                    for (k = 0; k < UPSCALE_FACTOR; k++)
                        for (j = 0; j < UPSCALE_FACTOR; j++) /* A felskálázás miatt minden pixelre be kell járni egy négyzetet */
                            if (PixelMap[i]) /* Aktív pixel esetén */
                                pixelRGBA(s, x + j, y + k, 0, 0, 0, 255); /* Legyen a pont fekete, ezeknél a telefonoknál így volt */
                            else /* Inaktív pixel esetén háttérszín */
                                pixelRGBA(s, x + j, y + k, BACKLIGHT, 255);
                    OldPixelMap[i] = PixelMap[i]; /* Az elõzõ képkocka tárolójába másolja át a mostanit */
                }
            }
            SDL_Flip(s); /* Újrarajzolás */
            break;
        case SDL_QUIT: /* Az operációs rendszer kilépés parancsára álljon le a futás, függetlenül a játék fázisától */
            run = 0;
            break;
        }
    }

    /** Kilépés **/
    EmptyEnemyList(&Enemies); /* Megmaradt ellenségek felszabadítása */
    EmptyScenery(&Scene); /* Megmaradt pályaelemek felszabadítása */
    EmptyShotList(&Shots); /* Megmaradt lövések felszabadítása */
    FreeDynamicGraphics(); /* Dinamikus grafikai objektumok felszabadítása */
    FreeDynamicEnemies(); /* Dinamikus ellenségek felszabadítása */
    SDL_RemoveTimer(t); /* Ez nem szükséges, csak a figyelmeztetést szünteti meg t felhasználatlanságára */
    SDL_PauseAudio(1); /* Hang megállítása */
    SDL_Quit();
    return 0;
}
