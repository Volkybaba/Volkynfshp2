#include "portlang.h"

#include <stdio.h>
#include <string.h>

#include "langtab.h"

// One row a string: English, French, German, Italian, Spanish, Swedish,
// Turkish, in LangEnum's order (LANG_TR is the seventh column). Latin-1, as retail's tables are, so each accent is its
// byte. "$NAME", "$N" and "$MAX" are filled in by whoever shows the line,
// and '@' breaks it, as in retail's.
static const char* const kStrings[PORTLANG_END - PORTLANG_FIRST][7] = {
    // PORTLANG_LOCAL_WIRELESS
    {"Local Wireless", "Sans fil local", "Lokal kabellos",
      "Wireless locale", "Inal\xe1mbrico local", "Lokalt tr\xe5" "dl\xf6st" , "Yerel Kablosuz"},
    // PORTLANG_LOCAL_WIRELESS_BLURB
    {"Up to eight players, each on their own 3DS.",
      "Jusqu'\xe0 huit joueurs, chacun sur sa 3DS.",
      "Bis zu acht Spieler, jeder an seinem 3DS.",
      "Fino a otto giocatori, ognuno con il suo 3DS.",
      "Hasta ocho jugadores, cada uno con su 3DS.",
      "Upp till \xe5tta spelare, var och en med sin 3DS." , "Sekize kadar oyuncu, her biri kendi cihazinda."},
    // PORTLANG_HOST
    {"Host", "Cr\xe9" "er", "Erstellen", "Crea", "Crear", "Skapa" , "Duzenle"},
    // PORTLANG_JOIN
    {"Join", "Rejoindre", "Beitreten", "Unisciti", "Unirse", "G\xe5 med" , "Katil"},
    // PORTLANG_HOST_OR_JOIN
    {"Host a race, or join one?",
      "Cr\xe9" "er une course ou en rejoindre une ?",
      "Ein Rennen erstellen oder einem beitreten?",
      "Creare una gara o unirsi a una?",
      "\xbf" "Crear una carrera o unirse a una?",
      "Skapa ett lopp eller g\xe5 med i ett?" , "Yaris duzenle veya birine katil?"},
    // PORTLANG_SEARCHING
    {"Looking for races...", "Recherche de courses...",
      "Suche nach Rennen...", "Ricerca di gare...", "Buscando carreras...",
      "Letar efter lopp..." , "Yarislar araniyor..."},
    // PORTLANG_FOUND
    {"Race: $NAME@Players: $N/$MAX", "Course : $NAME@Joueurs : $N/$MAX",
      "Rennen: $NAME@Spieler: $N/$MAX", "Gara: $NAME@Giocatori: $N/$MAX",
      "Carrera: $NAME@Jugadores: $N/$MAX", "Lopp: $NAME@Spelare: $N/$MAX" , "Yaris: $NAME@Oyuncular: $N/$MAX"},
    // PORTLANG_NEXT
    {"Next", "Suivant", "Weiter", "Avanti", "Siguiente", "N\xe4sta" , "Ileri"},
    // PORTLANG_WAITING_FOR_PLAYERS
    {"Waiting for players:", "En attente de joueurs :",
      "Warte auf Spieler:", "In attesa di giocatori:",
      "Esperando jugadores:", "V\xe4ntar p\xe5 spelare:" , "Oyuncular bekleniyor:"},
    // PORTLANG_WAITING_FOR_HOST
    {"Waiting for $NAME to start the race.",
      "En attente du lancement de la course par $NAME.",
      "Warte, bis $NAME das Rennen startet.",
      "In attesa che $NAME avvii la gara.",
      "Esperando a que $NAME inicie la carrera.",
      "V\xe4ntar p\xe5 att $NAME startar loppet." , "$NAME yarisi baslatmasi bekleniyor."},
    // PORTLANG_START
    {"Start", "Lancer", "Starten", "Avvia", "Empezar", "Starta" , "Baslat"},
    // PORTLANG_LINK_LOST
    {"The link to $NAME was lost.",
      "La liaison avec $NAME a \xe9t\xe9 perdue.",
      "Die Verbindung zu $NAME ist abgebrochen.",
      "Il collegamento con $NAME si \xe8 interrotto.",
      "Se perdi\xf3 la conexi\xf3n con $NAME.",
      "Anslutningen till $NAME br\xf6ts." , "$NAME ile baglanti koptu."},
    // PORTLANG_WIRELESS_OFF: with the console's wireless on, which
    // PORTLANG_WIRELESS_DISABLED says when it is not
    {"Local wireless could not start. Please try again.",
      "Impossible de lancer le sans-fil local. Veuillez r\xe9" "essayer.",
      "Lokal kabellos konnte nicht starten. Bitte versuche es erneut.",
      "Impossibile avviare il wireless locale. Riprova.",
      "No se pudo iniciar el modo inal\xe1mbrico local. Int\xe9ntalo de "
      "nuevo.",
      "Lokalt tr\xe5" "dl\xf6st kunde inte starta. F\xf6rs\xf6k igen." , "Yerel kablosuz baslatilamadi. Lutfen yeniden dene."},
    // PORTLANG_JOIN_FAILED
    {"Could not join the race.", "Impossible de rejoindre la course.",
      "Beitritt zum Rennen fehlgeschlagen.",
      "Impossibile unirsi alla gara.", "No se pudo unir a la carrera.",
      "Kunde inte g\xe5 med i loppet." , "Yarisa katilinamadi."},
    // PORTLANG_WAITING_FOR_CARS
    {"Waiting for a car from:", "En attente de la voiture de :",
      "Warte auf das Auto von:", "In attesa dell'auto di:",
      "Esperando el coche de:", "V\xe4ntar p\xe5 bil fr\xe5n:" , "Araba bekleniyor:"},
    // PORTLANG_CPU_RACERS
    {"CPU Racers", "Pilotes IA", "KI-Fahrer", "Piloti CPU", "Pilotos CPU",
      "Datorf\xf6rare" , "Bilgisayar Yariscilari"},
    // PORTLANG_CPU_RACERS_N
    {"CPU racers: $N", "Pilotes IA : $N", "KI-Fahrer: $N",
      "Piloti CPU: $N", "Pilotos CPU: $N", "Datorf\xf6rare: $N" , "Bilgisayar yariscilari: $N"},
    // PORTLANG_WIRELESS_DISABLED
    {"Wireless communication is turned off. Turn it on from the HOME "
      "Menu to play over Local Wireless.",
      "La communication sans fil est d\xe9sactiv\xe9" "e. Activez-la "
      "depuis le menu HOME pour jouer en sans fil local.",
      "Die Drahtlos-Kommunikation ist deaktiviert. Aktiviere sie im "
      "HOME-Men\xfc, um lokal kabellos zu spielen.",
      "La comunicazione wireless \xe8 disattivata. Attivala dal menu HOME "
      "per giocare in wireless locale.",
      "La conexi\xf3n inal\xe1mbrica est\xe1 desactivada. Act\xedvala "
      "desde el men\xfa HOME para jugar en modo inal\xe1mbrico local.",
      "Tr\xe5" "dl\xf6s kommunikation \xe4r avst\xe4ngd. Sl\xe5 p\xe5 den "
      "fr\xe5n HOME-menyn f\xf6r att spela lokalt tr\xe5" "dl\xf6st." , "Kablosuz iletisim kapali. Acmak icin Secenekler > Kablosuz "
      "ayarini degistir."},
    // PORTLANG_HOST_MODE: its buttons are the main menu's two modes,
    // retail's 10023 and 10022
    {"Which mode will you host?",
      "Quel mode voulez-vous cr\xe9" "er ?",
      "Welchen Modus willst du erstellen?",
      "Quale modalit\xe0 vuoi creare?",
      "\xbf" "Qu\xe9 modo quieres crear?",
      "Vilket l\xe4ge vill du skapa?" , "Hangi modda yaris duzenleyeceksin?"},
    // PORTLANG_RECONNECTING: $N is the time left, m:ss
    {"Reconnecting... $N", "Reconnexion... $N",
      "Verbindung wird wiederhergestellt... $N", "Riconnessione... $N",
      "Reconectando... $N", "\xc5teransluter... $N" , "Yeniden baglaniliyor... $N"},
    // PORTLANG_CPU_DRIVES
    {"The CPU drives $NAME's car.", "L'IA conduit la voiture de $NAME.",
      "Die KI f\xe4hrt das Auto von $NAME.", "La CPU guida l'auto di $NAME.",
      "La CPU conduce el coche de $NAME.", "Datorn k\xf6r $NAMEs bil." , "Bilgisayar $NAME arabasini suruyor."},
    // PORTLANG_CATCHING_UP: $N is how far, in percent
    {"Catching up with the race... $N%",
      "Rattrapage de la course... $N %",
      "Das Rennen wird aufgeholt... $N %",
      "Recupero della gara... $N%",
      "Alcanzando la carrera... $N %",
      "Kommer ikapp loppet... $N %" , "Yarisa yetisiliyor... $N%"},
    // PORTLANG_COMING_BACK: the same, $NAME's
    {"$NAME is coming back into the race... $N%",
      "$NAME revient dans la course... $N %",
      "$NAME kehrt ins Rennen zur\xfc" "ck... $N %",
      "$NAME sta rientrando in gara... $N%",
      "$NAME vuelve a la carrera... $N %",
      "$NAME kommer tillbaka till loppet... $N %" , "$NAME yarisa geri donuyor... $N%"},
    // PORTLANG_SURROUND: the 3DS's own Surround output, the name its System
    // Settings give it, where retail offered Dolby Pro Logic II (110011)
    {"Surround", "Surround", "Surround", "Surround", "Envolvente",
      "Surround" , "Surround"},
    // PORTLANG_RACER and PORTLANG_COP: Options' Controller, whose keys are
    // the racer's or the cop's car's (joyconfig.h)
    {"Racer", "Pilote", "Fahrer", "Pilota", "Piloto", "F\xf6rare" , "Yarisci"},
    {"Cop", "Policier", "Cop", "Poliziotto", "Poli", "Polis" , "Polis"},
    // PORTLANG_ROADBLOCK and PORTLANG_HELICOPTER: the cop's calls, in the
    // words retail's Be A Cop Blurb has for them (970001)
    {"Roadblock", "Barrage routier", "Stra\xdf" "ensperre",
      "Blocco stradale", "Corte de carretera", "V\xe4gsp\xe4rr" , "Barikat"},
    {"Helicopter", "H\xe9licopt\xe8re", "Helikopter", "Elicottero",
      "Helic\xf3ptero", "Helikopter" , "Helikopter"},
    // PORTLANG_CONTROLS_BLURB
#ifdef NFSHP2_R36S
#define CONTROLS_SELECT "\x86"
#else
#define CONTROLS_SELECT "A"
#endif
    {"Pick an action and press " CONTROLS_SELECT ", then press its new key. The action "
      "that had that key takes the old one. On the top line, Left and "
      "Right switch between the racer's keys and the cop's.",
      "Choisissez une action et appuyez sur " CONTROLS_SELECT ", puis sur son nouveau "
      "bouton. L'action qui avait ce bouton prend l'ancien. Sur la ligne "
      "du haut, Gauche et Droite passent des boutons du pilote \xe0 ceux "
      "du policier.",
      "W\xe4hle eine Aktion und dr\xfc" "cke " CONTROLS_SELECT ", dann ihre neue Taste. Die "
      "Aktion, die diese Taste hatte, bekommt die alte. In der obersten "
      "Zeile wechseln Links und Rechts zwischen den Tasten des Fahrers "
      "und denen des Cops.",
      "Scegli un'azione e premi " CONTROLS_SELECT ", poi il suo nuovo tasto. L'azione che "
      "aveva quel tasto prende quello vecchio. Sulla riga in alto, "
      "Sinistra e Destra passano dai tasti del pilota a quelli del "
      "poliziotto.",
      "Elige una acci\xf3n y pulsa " CONTROLS_SELECT ", y luego su nuevo bot\xf3n. La "
      "acci\xf3n que ten\xed" "a ese bot\xf3n se queda con el anterior. En "
      "la l\xednea de arriba, Izquierda y Derecha cambian entre los "
      "botones del piloto y los del poli.",
      "V\xe4lj en handling och tryck p\xe5 " CONTROLS_SELECT ", sedan p\xe5 dess nya knapp. "
      "Handlingen som hade knappen f\xe5r den gamla. P\xe5 \xf6versta "
      "raden v\xe4xlar V\xe4nster och H\xf6ger mellan f\xf6rarens knappar "
      "och polisens." , "Bir eylem sec ve " CONTROLS_SELECT "'ya bas, sonra yeni tusuna bas. O tusa sahip olan eylem eski tusu alir. Ust satirda, Sol ve Sag yariscinin tuslari ile polisin tuslari arasinda gecis yapar."},
#undef CONTROLS_SELECT
    // PORTLANG_CONTROLS_PRESS: $NAME is the action's label
    {"Press the new key for $NAME. START cancels.",
      "Appuyez sur le nouveau bouton pour $NAME. START annule.",
      "Dr\xfc" "cke die neue Taste f\xfcr $NAME. START bricht ab.",
      "Premi il nuovo tasto per $NAME. START annulla.",
      "Pulsa el nuevo bot\xf3n para $NAME. START cancela.",
      "Tryck p\xe5 den nya knappen f\xf6r $NAME. START avbryter." , "$NAME icin yeni tusa bas. START iptal eder."},
    // PORTLANG_SAVE_CHANGES: the save prompt of a screen left with changes
    {"Save your changes to the SD Card?",
      "Sauvegarder vos modifications sur la carte SD ?",
      "Deine \xc4nderungen auf der SD Card speichern?",
      "Salvare le modifiche sulla scheda SD?",
      "\xbfGuardar los cambios en la tarjeta SD?",
      "Spara dina \xe4ndringar p\xe5 SD-kortet?" , "Degisiklikleri SD Karta kaydet?"},
    // PORTLANG_SAVE_CHANGES_RACE: the debug menu's Start Race
    {"Save your changes to the SD Card before the race?",
      "Sauvegarder vos modifications sur la carte SD avant la course ?",
      "Deine \xc4nderungen vor dem Rennen auf der SD Card speichern?",
      "Salvare le modifiche sulla scheda SD prima della gara?",
      "\xbfGuardar los cambios en la tarjeta SD antes de la carrera?",
      "Spara dina \xe4ndringar p\xe5 SD-kortet f\xf6re loppet?" , "Yaristan once degisiklikleri SD Karta kaydet?"},
    // PORTLANG_EXIT_GAME: the main menu's Exit Game confirmation title.
    // The English column is what this device shows, as the whole UI is
    // Turkish through the English file; the rest is the English fallback.
    {"Exit Game",
      "Exit Game",
      "Exit Game",
      "Exit Game",
      "Exit Game",
      "Exit Game" , "Oyundan Cik"},
    // PORTLANG_WIRELESS_BLURB: the Options screen's Wireless row's blurb
    // (R36S). Same pattern: the English column is the Turkish the device
    // shows, the rest the English fallback.
    {"Turn wireless on or off.",
      "Turn wireless on or off.",
      "Turn wireless on or off.",
      "Turn wireless on or off.",
      "Turn wireless on or off.",
      "Turn wireless on or off." , "Kablosuz iletisimi ac veya kapat."},
    // PORTLANG_WIRELESS_ROW_ON: the Options screen's Wireless row, on (R36S).
    // All seven columns filled (v33): the row follows the selected
    // language, like PORTLANG_LOCAL_WIRELESS does.
    {"Wireless: On",
      "Sans fil : Activ\xe9",
      "Kabellos: Ein",
      "Wireless: On",
      "Inal\xe1mbrico: S\xed",
      "Tr\xe5" "dl\xf6st: P\xe5" , "Kablosuz: Acik"},
    // PORTLANG_WIRELESS_ROW_OFF: the Options screen's Wireless row, off (R36S).
    {"Wireless: Off",
      "Sans fil : D\xe9sactiv\xe9",
      "Kabellos: Aus",
      "Wireless: Off",
      "Inal\xe1mbrico: No",
      "Tr\xe5" "dl\xf6st: Av" , "Kablosuz: Kapali"},
    // PORTLANG_EXIT_CONFIRM: the main menu's Exit Game popup question (R36S).
    {"Are you sure you want to quit the game?",
      "\xca" "tes-vous s\xfbr de vouloir quitter le jeu ?",
      "M\xf6" "chtest du das Spiel wirklich beenden?",
      "Sei sicuro di voler uscire dal gioco?",
      "\xbf" "Est\xe1s seguro de que quieres salir del juego?",
      "\xc4r du s\xe4ker p\xe5 att du vill avsluta spelet?" ,
      "Oyundan cikmak istedigine emin misin?"},
    // PORTLANG_EXIT_BLURB: the main menu's Exit Game row's blurb (R36S).
    {"Select to quit the game.",
      "S\xe9lectionner pour quitter le jeu.",
      "Zum Beenden des Spiels ausw\xe4hlen.",
      "Seleziona per uscire dal gioco.",
      "Selecciona para salir del juego.",
      "V\xe4lj f\xf6r att avsluta spelet." ,
      "Oyundan cikmak icin sec."},
    // PORTLANG_CHANGE: the Controller page's bottom row (R36S).
    {"Change",
      "Modifier",
      "Andern",
      "Cambia",
      "Cambiar",
      "Andra" ,
      "Degistir"},
    // PORTLANG_BACK: the Controller page's bottom row (R36S).
    {"Back",
      "Retour",
      "Zuruck",
      "Indietro",
      "Atras",
      "Tillbaka" ,
      "Geri"},
    // PORTLANG_DEFAULTS: the Controller page's bottom row (R36S).
    {"Defaults",
      "Defaut",
      "Standard",
      "Predefiniti",
      "Predeterminado",
      "Standard" ,
      "Varsayilan"},
    // PORTLANG_RACER_COP: the Controller page's bottom row, table row (R36S).
    {"Racer/Cop",
      "Pilote/Policier",
      "Fahrer/Polizist",
      "Pilota/Poliziotto",
      "Piloto/Policia",
      "Forare/Polis" ,
      "Yarisci/Polis"},
    // PORTLANG_CANCEL: the Controller page's bottom row, waiting (R36S).
    {"Cancel",
      "Annuler",
      "Abbrechen",
      "Annulla",
      "Cancelar",
      "Avbryt" ,
      "Iptal"},
};

// Retail's memory card messages and the Load/Save screen's help lines,
// said for the 3DS: retail's text in each language with the card's words
// put as the SD Card's and the MEMORY CARD slot dropped
// (docs/formats/language-table.md, "The port's wording"). The slot went
// first in the two that need space, so they take the KB as "%2$d"
// (memcard.c's formatPrompt). The help lines keep their buttons: 0x82 0x83
// the D-pad, 0x86 0x80 Cross and START.
static const struct {
    int32_t     id;
    const char* text[7];
} kRetail[] = {
    { 150004, {"SD Card",
        "Carte SD",
        "SD Card",
        "Scheda SD",
        "Tarjeta SD",
        "SD-kort"
    , "SD Kart"} },
    { 151001, {"Do not remove the SD Card or turn the power off. @ @ Loading game "
        "settings.",
        "Ne retirez pas la carte SD et n'\xe9teignez pas la console. @ @ "
        "Chargement des param\xe8tres de la partie...",
        "Die SD Card nicht entfernen. Konsole nicht ausschalten. @ "
        "@Spieleinstellungen werden geladen.",
        "Non rimuovere la scheda SD. Non spegnere la console. @ @ "
        "Caricamento impostazioni di gioco.",
        "No retires la tarjeta SD. No apagues la consola. @ @ Cargando "
        "par\xe1metros del juego.",
        "Ta inte ur ditt SD-kort. Sl\xe5 inte av str\xf6mmen. @ @ Laddar "
        "spelinst\xe4llningar."
    , "SD Karti cikarma ve gucu kapatma. @ @ Oyun ayarlari "
        "yukleniyor."} },
    { 151003, {"No saved game settings found on the SD Card",
        "Aucun param\xe8tre sauvegard\xe9 n'a \xe9t\xe9 d\xe9tect\xe9 sur "
        "la carte SD.",
        "Auf der SD Card wurden keine gespeicherten Einstellungen gefunden.",
        "Nessuna partita salvata sulla scheda SD",
        "No se han encontrado par\xe1metros del juego guardados en la "
        "tarjeta SD.",
        "Inga sparade spelinst\xe4llningar hittades p\xe5 ditt SD-kort"
    , "SD Kartta kayitli oyun ayari bulunamadi"} },
    { 151004, {"Saving game settings to the SD Card.@Do not remove the SD Card or "
        "turn the power off.",
        "Sauvegarde des param\xe8tres de la partie sur la carte SD en "
        "cours. Ne retirez pas la carte SD et n'\xe9teignez pas la console.",
        "Spieldaten werden auf der SD Card gespeichert. Die SD Card nicht "
        "entfernen. Konsole nicht ausschalten.",
        "Salvataggio del file di gioco sulla scheda SD@Non rimuovere la "
        "scheda SD. Non spegnere la console.",
        "Guardando par\xe1metros del juego en la tarjeta SD.@No retires la "
        "tarjeta SD. No apagues la consola.",
        "Sparar spelinst\xe4llningar till SD-kortet.@Ta inte ur ditt "
        "SD-kort. Sl\xe5 inte av str\xf6mmen."
    , "Oyun ayarlari SD Karta kaydediliyor.@SD Karti cikarma ve "
        "gucu kapatma."} },
    { 151006, {"No SD Card detected. @ @Would you like to retry?",
        "Aucune carte SD n'a \xe9t\xe9 d\xe9tect\xe9" "e.@Voulez-vous "
        "recommencer ?",
        "Es wurde keine SD Card gefunden.@ @M\xf6" "chtest du es erneut "
        "versuchen?",
        "Nessuna scheda SD rilevata. @ @Vuoi riprovare?",
        "No se ha detectado ninguna tarjeta SD. @\xbfQuieres volver a "
        "intentarlo?",
        "Inget SD-kort hittat. @ @Vill du f\xf6rs\xf6ka igen?"
    , "SD Kart bulunamadi. @ @Tekrar denemek ister misin?"} },
    { 151007, {"The SD Card is unformatted. Would you like to format it?",
        "La carte SD n'est pas format\xe9" "e. Souhaitez-vous la formater ?",
        "Die SD Card ist nicht formatiert. Soll jetzt formatiert werden?",
        "La scheda SD non \xe8 formattata. Vuoi formattarla?",
        "La tarjeta SD no est\xe1 formateada. \xbfQuieres formatearla?",
        "Ditt SD-kort \xe4r inte formaterat. Vill du formatera det?"
    , "SD Kart bicimlendirilmemis. Bicimlendirmek ister misin?"} },
    { 151008, {"Do not remove the SD Card. @ @ Formatting...",
        "Ne retirez pas la carte SD. @ @ Formatage en cours...",
        "Die SD Card nicht entfernen. @ @ Formatierung wird "
        "durchgef\xfchrt ...",
        "Non rimuovere la scheda SD. @ @ Formattazione in corso...",
        "No retires la tarjeta SD. @ @ Formateando...",
        "Ta inte ur ditt SD-kort. @ @ Formaterar..."
    , "SD Karti cikarma. @ @ Bicimlendiriliyor..."} },
    { 151009, {"The SD Card was formatted successfully!",
        "Formatage de la carte SD r\xe9ussi !",
        "Die SD Card wurde erfolgreich formatiert!",
        "La scheda SD \xe8 stata formattata correttamente!",
        "\xa1Se ha formateado la tarjeta SD!",
        "Ditt SD-kort formaterat!"
    , "SD Kart basariyla bicimlendirildi!"} },
    { 151010, {"Format failed for the SD Card. @ @ Do you want to try again?",
        "Echec du formatage de la carte SD. @ @ Souhaitez-vous "
        "r\xe9" "essayer ?",
        "Die Formatierung der SD Card ist fehlgeschlagen. @ @ "
        "M\xf6" "chtest du es erneut versuchen?",
        "Formattazione fallita della scheda SD. @ @ Vuoi riprovare?",
        "Error de formato para la tarjeta SD. @ @ \xbfQuieres volver a "
        "intentarlo?",
        "Kunde inte formatera ditt SD-kort. @ @ Vill du f\xf6rs\xf6ka igen?"
    , "SD Kart bicimlendirilemedi. @ @ Tekrar denemek ister misin?"} },
    { 151011, {"There is not enough space to save the game on the SD Card. Need "
        "For Speed: Hot Pursuit 2 requires %2$d KB free. Do you want to "
        "try again?",
        "La carte SD poss\xe8" "de un espace insuffisant. La sauvegarde "
        "des donn\xe9" "es de Need For Speed : Poursuite Infernale 2 "
        "n\xe9" "cessite %2$dKB d'espace libre. Souhaitez-vous "
        "r\xe9" "essayer ?",
        "Es ist nicht gen\xfcgend freier Speicher vorhanden, um das Spiel "
        "auf der SD Card zu speichern. F\xfcr Need For Speed: Hot Pursuit "
        "2 werden %2$d KB freier Speicherplatz ben\xf6tigt. M\xf6" "chtest "
        "du es noch mal versuchen?",
        "Spazio insufficiente sulla scheda SD. Need For Speed: Hot Pursuit "
        "2 richiede %2$d KB liberi. Vuoi riprovare?",
        "No hay espacio suficiente para guardar la partida en la tarjeta "
        "SD. Need For Speed: Hot Pursuit 2 requiere %2$d KB libres. "
        "\xbfQuieres volver a intentarlo?",
        "Det finns inte tillr\xe4" "ckligt med minne f\xf6r att spara "
        "spelet p\xe5 ditt SD-kort. Need For Speed: Hot Pursuit 2 "
        "kr\xe4ver %2$d KB ledigt utrymme. Vill du f\xf6rs\xf6ka igen?"
    , "There is not enough space to save the game on the SD Card. Need "
        "For Speed: Hot Pursuit 2 requires %2$d KB free. Do you want to "
        "try again?"} },
    { 151012, {"Save failed for the SD Card.@Do you want to try again?",
        "Echec de la sauvegarde sur la carte SD.@Souhaitez-vous "
        "r\xe9" "essayer ?",
        "Die Daten konnten nicht auf der SD Card gespeichert "
        "werden.@M\xf6" "chtest du es noch mal versuchen?",
        "Salvataggio fallito sulla scheda SD.@Vuoi riprovare?",
        "Error de guardado en la tarjeta SD.@\xbfQuieres volver a "
        "intentarlo?",
        "Kunde inte spara p\xe5 SD-kortet.@Vill du f\xf6rs\xf6ka igen?"
    , "SD Karta kaydetme basarisiz oldu.@Tekrar denemek ister misin?"} },
    { 151013, {"Checking the SD Card",
        "V\xe9rification de la carte SD...",
        "Die Suche nach einer SD Card wurde gestartet.",
        "Controllo della scheda SD",
        "Comprobando la tarjeta SD.",
        "Letar efter SD-kort"
    , "SD Kart kontrol ediliyor"} },
    { 151014, {"A new SD Card was found.",
        "Une nouvelle carte SD a \xe9t\xe9 d\xe9tect\xe9" "e.",
        "Es wurde eine neue SD Card gefunden.",
        "\xc8 stata rilevata una nuova scheda SD.",
        "Se ha encontrado una tarjeta SD nueva.",
        "Ett nytt SD-kort hittades."
    , "Yeni bir SD Kart bulundu."} },
    { 151015, {"No SD Card detected.@ Would you like to retry loading?",
        "Aucune carte SD n'a \xe9t\xe9 d\xe9tect\xe9" "e.",
        "Es wurde keine SD Card gefunden.@M\xf6" "chtest du erneut "
        "versuchen, zu laden?",
        "Nessuna scheda SD rilevata. Vuoi riprovare il caricamento?",
        "No se ha detectado ninguna tarjeta SD. \xbfQuieres volver a "
        "intentar cargar?",
        "Inget SD-kort hittat.@ Vill du f\xf6rs\xf6ka ladda igen?"
    , "SD Kart bulunamadi.@Yuklemeyi tekrar denemek ister misin?"} },
    { 151016, {"Invalid SD Card @ @ The inserted device is not an SD Card.",
        "Carte SD non valide @ @ L'accessoire ins\xe9r\xe9 n'est pas une "
        "carte SD.",
        "Ung\xfcltige SD Card @ @ Der eingesteckte Gegenstand ist keine SD "
        "Card.",
        "Scheda SD non valida @ @ La periferica inserita non \xe8 una "
        "scheda SD.",
        "Tarjeta SD no v\xe1lida @ @ El dispositivo insertado no es una "
        "tarjeta SD.",
        "Ogiltigt SD-kort @ @ Enheten \xe4r inget SD-kort."
    , "Gecersiz SD Kart @ @ Takili cihaz SD Kart degil."} },
    { 151017, {"No SD Card detected.",
        "Aucune carte SD n'a \xe9t\xe9 d\xe9tect\xe9" "e.",
        "Es wurde keine SD Card gefunden.",
        "Nessuna scheda SD individuata.",
        "No se ha detectado ninguna tarjeta SD.",
        "Inget SD-kort hittat."
    , "SD Kart bulunamadi."} },
    { 151018, {"Load failed for the SD Card.@Would you like to try again?",
        "Echec du chargement \xe0 partir de la carte SD. @Souhaitez-vous "
        "r\xe9" "essayer ?",
        "Laden von der SD Card fehlgeschlagen.@M\xf6" "chtest du es noch "
        "mal versuchen?",
        "Caricamento fallito della scheda SD.@Vuoi riprovare?",
        "Error al cargar la tarjeta SD. @\xbfQuieres volver a intentarlo?",
        "Kunde inte ladda ditt SD-kort.@Vill du f\xf6rs\xf6ka igen?"
    , "SD Karttan yukleme basarisiz oldu.@Tekrar denemek ister misin?"} },
    { 151020, {"Overwrite existing game settings@on the SD Card?",
        "Ecraser les param\xe8tres sauvegard\xe9s@sur la carte SD ?",
        "M\xf6" "chtest du die aktuellen Spieleinstellungen@auf der SD "
        "Card \xfc" "berschreiben?",
        "Sovrascrivere le impostazioni di gioco@esistenti sulla scheda SD?",
        "\xbfReemplazar los par\xe1metros del juego existentes en la "
        "tarjeta SD?",
        "Skriv \xf6ver nuvarande spelinst\xe4llningar p\xe5 SD-kortet?"
    , "SD Karttaki mevcut oyun ayarlarinin uzerine yazilsin mi?"} },
    { 151022, {"Current settings will be overwritten by the game settings on the "
        "SD Card. Continue?",
        "Les param\xe8tres actuels seront remplac\xe9s par ceux "
        "pr\xe9sents sur la carte SD. Continuer ?",
        "Die aktuellen Einstellungen werden durch die Spieleinstellungen "
        "auf der SD Card \xfc" "berschrieben. Fortfahren?",
        "Le impostazioni attuali saranno sostituite dalle impostazioni di "
        "gioco presenti sulla scheda SD. Continuare?",
        "Los par\xe1metros actuales ser\xe1n reemplazados por los "
        "par\xe1metros del juego en la tarjeta SD. \xbf" "Deseas continuar?",
        "Nuvarande inst\xe4llningar kommer att skrivas \xf6ver av "
        "spelinst\xe4llningarna p\xe5 ditt SD-kort. Forts\xe4tt?"
    , "Mevcut ayarlar SD Karttaki oyun ayarlariyla degistirilecek. "
        "Devam edilsin mi?"} },
    { 151023, {"Create Saved Game on the SD Card?",
        "Cr\xe9" "er une sauvegarde sur la carte SD ?",
        "Gespeichertes Spiel auf der SD Card erstellen.",
        "Creazione del file di salvataggio sulla scheda SD.",
        "\xbf" "Crear partida guardada en la tarjeta SD?",
        "Skapa sparad spelomg\xe5ng p\xe5 ditt SD-kort?"
    , "SD Karta kayitli oyun olusturulsun mu?"} },
    { 151024, {"Load failed for the SD Card.",
        "Echec du chargement \xe0 partir de la carte SD.",
        "Laden von der SD Card fehlgeschlagen.",
        "Caricamento fallito dalla scheda SD.",
        "Error de cargado en la tarjeta SD.",
        "Kunde inte ladda ditt SD-kort."
    , "SD Karttan yukleme basarisiz oldu."} },
    { 151025, {"No SD Card detected.",
        "Aucune carte SD n'a \xe9t\xe9 d\xe9tect\xe9" "e.",
        "Keine SD Card gefunden.",
        "Nessuna scheda SD individuata.",
        "No se ha detectado ninguna tarjeta SD.",
        "Inget SD-kort hittat."
    , "SD Kart bulunamadi."} },
    { 151027, {"No SD Card detected. Need For Speed: Hot Pursuit 2 requires %2$d "
        "KB free. Do you want to try again?",
        "Aucune carte SD n'a \xe9t\xe9 d\xe9tect\xe9" "e. Need For Speed : "
        "Poursuite Infernale 2 n\xe9" "cessite %2$dKB. Voulez-vous "
        "r\xe9" "essayer ?",
        "Keine SD Card gefunden. F\xfcr Need For Speed: Hot Pursuit 2 "
        "werden %2$d KB freier Speicherplatz ben\xf6tigt. M\xf6" "chtest "
        "du es noch mal versuchen?",
        "Nessuna scheda SD rilevata. Need For Speed: Hot Pursuit 2 "
        "richiede almeno %2$d KB di spazio disponibile. Vuoi riprovare?",
        "No se ha detectado ninguna tarjeta SD. Need For Speed: Hot "
        "Pursuit 2 requiere %2$d KB libres. \xbfQuieres intentarlo de "
        "nuevo?",
        "Inget SD-kort hittat. Need For Speed: Hot Pursuit 2 kr\xe4ver "
        "%2$d KB ledigt utrymme. Vill du f\xf6rs\xf6ka igen?"
    , "SD Kart bulunamadi. Need For Speed: Hot Pursuit 2 icin %2$d "
        "KB bos alan gerekiyor. Tekrar denemek ister misin?"} },
    { 151101, {"The SD Card is unformatted.",
        "La carte SD n'est pas format\xe9" "e.",
        "Die SD Card ist nicht formatiert.",
        "La scheda SD non \xe8 formattata.",
        "La tarjeta SD no est\xe1 formateada.",
        "Ditt SD-kort \xe4r inte formaterat"
    , "SD Kart bicimlendirilmemis."} },
    { 152001, {"Do not remove the SD Card. @ @ Unformatting...",
        "Ne retirez pas la carte SD. @ @ Annulation du formatage en "
        "cours...",
        "Die SD Card nicht entfernen. @ @ Formatierung wird "
        "r\xfc" "ckg\xe4ngig gemacht ...",
        "Non rimuovere la scheda SD. @ @ Deformattazione in corso...",
        "No retires la tarjeta SD. @ @ Quitando formato...",
        "Ta inte ur ditt SD-kort. @ @ Avformaterar..."
    , "SD Karti cikarma. @ @ Bicimlendirme geri aliniyor..."} },
    { 800070, {"\x82 \x83 Select SD Card function",
        "\x82 \x83 Parcourir les options de carte SD",
        "\x82 \x83 SD Card-Funktion ausw\xe4hlen",
        "\x82 \x83 seleziona funzione scheda SD",
        "\x82 \x83 Seleccionar funci\xf3n tarjeta SD",
        "\x82 \x83 V\xe4lj SD-kort-funktion"
    , "\x82 \x83 SD Kart islevi sec"} },
    { 800071, {"\x86 \x80 Activate SD Card function",
        "\x86 \x80 S\xe9lectionner l'option de carte SD",
        "\x86 \x80 SD Card-Funktion aktivieren",
        "\x86 \x80 attiva funzione scheda SD",
        "\x86 \x80 Activar funci\xf3n tarjeta SD",
        "\x86 \x80 Aktivera SD-kort-funktion"
    , "\x86 \x80 SD Kart islevini etkinlestir"} },
};

const char* portlang_get(int32_t languageEnum, int32_t id, size_t* outLen)
{
    static char        number[PORTLANG_END - PORTLANG_FIRST][12];
    const char* const* row;
    const char*        s;

    if (id < PORTLANG_FIRST || id >= PORTLANG_END)
        return NULL;
    row = kStrings[id - PORTLANG_FIRST];
    if (languageEnum == LANG_ID_NUM) {
        s = number[id - PORTLANG_FIRST];
        if (!s[0])
            snprintf(number[id - PORTLANG_FIRST], sizeof number[0], "%ld",
                     (long)id);
    } else if (languageEnum == LANG_LONGEST) {
        int i;

        s = row[0];
        for (i = 1; i < 7; i++)
            if (strlen(row[i]) > strlen(s))
                s = row[i];
    } else {
        s = row[languageEnum >= LANG_EN && languageEnum <= LANG_TR
                    ? languageEnum : LANG_EN];
    }
    if (outLen)
        *outLen = strlen(s);
    return s;
}

// A kLanguages list index (main.c) as the LangEnum portlang_get wants.
// The list puts Turkish first; the enum puts English first, so the two
// only agree by accident for the languages in between. The table is the
// kLanguages order against LangEnum (langtab.h); if either order ever
// moves, this is the one place to move with it.
const char* portlang_get_list(int listIndex, int32_t id, size_t* outLen)
{
    static const int32_t kListToEnum[] = {
        LANG_TR, LANG_EN, LANG_FR, LANG_DE, LANG_IT, LANG_ES, LANG_SE,
        LANG_LONGEST, LANG_ID_NUM,
    };

    if (listIndex < 0 ||
        listIndex >= (int)(sizeof kListToEnum / sizeof kListToEnum[0]))
        return portlang_get(LANG_EN, id, outLen);
    return portlang_get(kListToEnum[listIndex], id, outLen);
}

const char* portlang_retail(int32_t languageEnum, int32_t id, size_t* outLen)
{
    size_t i;

    if (languageEnum < LANG_EN || languageEnum > LANG_TR)
        return NULL;
    for (i = 0; i < sizeof kRetail / sizeof kRetail[0]; i++)
        if (kRetail[i].id == id) {
            const char* s = kRetail[i].text[languageEnum];

            if (outLen)
                *outLen = strlen(s);
            return s;
        }
    return NULL;
}
