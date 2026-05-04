# PROJEKT IPK-2 Spoľahlivý prenos cez UDP  
**Login:** xbugarm00

## 1. Prehľad projektu
Tento projekt implementuje spoľahlivý transportný protokol postavený nad UDP, inšpirovaný protokolom TCP. Cieľom je zabezpečiť spoľahlivé, zoradené a integritou chránené doručovanie bajtového prúdu v prostredí s nekvalitným sieťovým pripojením (strata paketov, duplikácia, zmena poradia). Aplikácia je napísaná v C++.

## 2. Inštrukcie k zostaveniu a spusteniu
Projekt využíva `Makefile` a je plne kompatibilný s referenčným prostredím Nix.

**Kompilácia:**
```bash
make                  
```
## Spustenie testov:
```bash
make test             # Spustí sadu C++ Unit testov (CLI, Checksum, Siet)
chmod +x integration.sh
./integration.sh      # Spustí komplexné integračné testy v Bashi
```
## Ako používať:
```bash 
**Server**
./ipk-rdt -s -p PORT [-a ADDRESS] [-o OUTPUT] [-w TIMEOUT] [-h | --help]
**Klient**
./ipk-rdt -c -a HOST -p PORT [-i INPUT] [-w TIMEOUT] [-h | --help]
```
##### Popis argumentov:
* -h alebo --help: Vypíše inštrukcie k použitiu na štandardný výstup (stdout) a bezpečne ukončí program s exit kódom 0.

* -s: Spustí aplikáciu v režime servera (prijímacia strana).
* -c: Spustí aplikáciu v režime klienta (odosielacia strana). Upozornenie: Musí byť špecifikovaný presne jeden z prepínačov -c alebo -s.
* -p PORT: Definuje číslo UDP portu, na ktorom má prebiehať komunikácia.
* -a ADDRESS (Server mode): Špecifikuje lokálnu IP adresu pre bind. Ak je vynechaný, server počúva na všetkých dostupných lokálnych adresách.
* -a HOST (Client mode): Špecifikuje cieľovú IP adresu (IPv4/IPv6) alebo hostname servera.
* -i INPUT: Definuje vstupný súbor na odoslanie. Ak je vynechaný, alebo je špecifikovaný ako pomlčka (-), klient číta dáta zo štandardného vstupu (stdin).
* -o OUTPUT: Definuje výstupný súbor na uloženie prijatých dát (vytvorí ho alebo prepíše). Ak je vynechaný, alebo je špecifikovaný ako pomlčka (-), server vypisuje prijaté dáta na štandardný výstup (stdout).
* -w TIMEOUT: Definuje maximálny povolený čas v celých sekundách, počas ktorého sa čaká na platný posun v protokole (protokolárny progres). Ak parameter nie je zadaný, predvolená hodnota je 1 sekunda. Pri prekročení tohto času sa aplikácia ukončí s nenulovým návratovým kódom.

##### Doplnkové parametre
* **Timeout (`-w`):** Maximálny interval bez protokolu v sekundách (predvolene 1s). Ak po dobu špecifikovanú v `-w` nenastane žiadny pokrok (nový unikátny paket alebo ACK), aplikácia sa ukončí s nenulovým kódom.
    * *Príklad:* `./ipk-rdt -s -p 9000 -w 5`
* **Adresa (`-a`):** U servera špecifikuje bind adresu (voliteľné), u klienta cieľový host alebo IP adresu (povinné).
* **Nápoveda (`-h` | `--help`):** Vypíše inštrukcie k použitiu a ukončí program s kódom 0.

### 1. Prenos zo súboru do súboru
V tomto scenári server ukladá dáta do špecifikovaného súboru a klient číta dáta zo súboru.
* **Server:** `./ipk-rdt -s -p 9000 -o received.bin`
* **Klient:** `./ipk-rdt -c -a 127.0.0.1 -p 9000 -i sample.bin`

### 2. Prenos zo štandardného vstupu (stdin) na štandardný výstup (stdout)
Ak nie je špecifikovaný prepínač `-i` u klienta alebo `-o` u servera, aplikácie využívajú štandardné prúdy.
* **Server:** `./ipk-rdt -s -p 9000`
* **Klient:** `printf 'IPK\n' | ./ipk-rdt -c -a 127.0.0.1 -p 9000`

### 3. Prenos zo štandardného vstupu (stdin) do súboru
Kombinácia rúry (pipe) na strane klienta a zápisu do súboru na strane servera.
* **Server:** `./ipk-rdt -s -p 9000 -o output.data`
* **Klient:** `cat input.data | ./ipk-rdt -c -a localhost -p 9000`

### 4. Prenos zo súboru na štandardný výstup (stdout)
* **Server:** `./ipk-rdt -s -p 9000`
* **Klient:** `./ipk-rdt -c -a localhost -p 9000 -i sample.bin`

## 3. Formát paketov a hlavičky

Protokol používa pevne stanovenú hlavičku o veľkosti **17 bajtov**. Celková veľkosť UDP datagramu (hlavička + dáta) je prísne obmedzená na maximálne **1200 bajtov**.

| Pole | Veľkosť (B) | Typ | Popis |
| :--- | :--- | :--- | :--- |
| `connection_id` | 4 | `uint32_t` | Unikátny kľúč relácie pre multiplexing. |
| `seq_num` | 4 | `uint32_t` | Sekvenčné číslo odoslaného paketu. |
| `ack_num` | 4 | `uint32_t` | Číslo potvrdzovaného/očakávaného paketu. |
| `data_len` | 2 | `uint16_t` | Dĺžka užitočných dát (payloadu) v aktuálnom pakete. |
| `checksum` | 2 | `uint16_t` | Kontrolný súčet (RFC 1071). |
| `ack_flag` | 1 | `uint8_t` | Typ/Stav paketu (HELLO, DATA, FINAL, ACK). |

Návrh tohto formátu vychádza z potrieb spoľahlivého prenosu a limitácií rodiny protokolov TCP/IP:
* **Limit 1200 bajtov:** Táto hodnota bola zvolená na bezpečné predchádzanie fragmentácii na vrstve IP. Štandardná hodnota MTU v sieťach je 1500 bajtov. Ak odpočítame veľkosť IPv4 hlavičky (min. 20 B) a UDP hlavičky (8 B), limit 1200 bajtov necháva dostatočnú rezervu aj pre zložitejšie sieťové tunely (napr. VPN alebo IPv6 s rozširujúcimi hlavičkami), čím sa garantuje, že paket dorazí k cieľu vcelku.
* **Typy sekvenčných čísel (`uint32_t`):** Pre `seq_num` a `ack_num` bol zvolený 32-bitový formát (namiesto menšieho 16-bitového). To umožňuje bezproblémový prenos obrovských súborov bez rizika rýchleho pretečenia (tzv. *sequence wrapping*), čo udržuje logiku posúvania okna (Sliding Window) jednoduchú a bezpečnú.
* **Identifikátor pripojenia (`connection_id`):** Pridaný na ochranu prenosu. Pri použití stavového protokolu nad bezstavovým UDP hrozí, že oneskorený paket z predchádzajúceho (už ukončeného) prenosu by poškodil nový prenos. 32-bitový náhodne generovaný kľúč tomuto spoľahlivo zabraňuje.
* **Dĺžka dát (`data_len`):** Hoci je maximálny payload pevný (1183 bajtov), posledný paket prenosu býva zvyčajne menší. Prijímač vďaka tomuto poľu vie, koľko presne bajtov má zapísať do výstupného súboru, aby nevznikali nulové "padding" dáta na konci súboru.
* **Absencia "Paddingu" v štruktúre:** V jazyku C++ bola hlavička implementovaná pomocou direktívy `__attribute__((packed))`. Vďaka tomu kompilátor nevkladá medzi premenné žiadne prázdne výplňové bajty, čo zaisťuje prenosnosť a bezchybné dekódovanie na rôznych procesorových architektúrach.

## 4. Nadviazanie a ukončenie spojenia

Keďže UDP je vo svojej podstate bezstavový protokol, naša aplikácia nad ním implementuje vlastné explicitné riadenie relácie pomocou "handshaku":

* **Nadviazanie (Establishment):** Klient vygeneruje náhodné 32-bitové `connection_id` a odošle inicializačný paket `HELLO`. Server po jeho prijatí odpovie paketom `HELLO_ACK`, čím potvrdí prijatie identifikátora a pripravenosť na príjem dát.
* **Ukončenie (Termination):** Po úspešnom prenesení a potvrdení všetkých dát odošle klient paket `FINAL`. Server po zápise posledných dátových blokov odpovie paketom `FINAL_ACK` a bezpečne ukončí svoj proces. Klient sa ukončí bezprostredne po prijatí tohto potvrdenia.

### Zdôvodnenie návrhu (Design Decisions)
* **Prečo 2-cestný handshake?** Na rozdiel od plnohodnotného TCP (ktoré používa 3-cestný handshake), tu ide výhradne o jednosmerný prenos dát (od klienta k serveru). Dvojcestná komunikácia (`HELLO` -> `HELLO_ACK`) je úplne postačujúca na overenie dostupnosti servera a dohodnutie spoločného ID relácie predtým, než klient začne saturavať sieť dátami.
* **Bezpečné ukončenie:** Mechanizmus s `FINAL` paketom garantuje, že spojenie sa neukončí predčasne. Ak by klient len "skončil", server by nemal ako vedieť, či má čakať na ďalšie dáta (a spadol by na Timeout).

---

## 5. Stratégia sekvenovania a potvrdzovania

Pre zabezpečenie maximálnej sieťovej priepustnosti bol namiesto primitívnej metódy "Stop-and-Wait" (Pošli a čakaj) implementovaný mechanizmus **Pipelining** s využitím metódy **Sliding Window (Posuvné okno)** v štýle **Go-Back-N**.

* **Posuvné okno:** Je zvolená pevná veľkosť okna `WINDOW_SIZE = 25` paketov.
* **Potvrdzovanie (ACK):** Server využíva **kumulatívne potvrdenia**. V hlavičke paketu `DATA_ACK` v poli `ack_num` server vždy odosiela číslo posledného úspešne prijatého paketu v správnom poradí. Akákoľvek medzera v poradí spôsobí zopakovanie posledného dobrého `ack_num`.

### Zdôvodnenie návrhu (Design Decisions)
* **Prečo Sliding Window namiesto Stop-and-Wait?** Metóda Stop-and-Wait pošle 1 paket a čaká na 1 ACK. Na sieti s vysokým oneskorením (RTT) to drasticky znižuje prenosovú rýchlosť. Vďaka nášmu posuvnému oknu s veľkosťou 25 dokáže klient udržať v sieti ("v obehu") takmer 30 KB nepotvrdených dát naraz, čo efektívne využíva dostupnú šírku pásma.
* **Výber Go-Back-N (GBN):** GBN bol zvolený pre jeho jednoduchosť na strane prijímača (Servera). Prijímač nemusí implementovať zložité a pamäťovo náročné buffre pre pakety, ktoré dorazia mimo poradia. Udržuje si iba premennú `expected_seq` a všetko, čo do nej nezapadá, jednoducho zahodí.
* **Sila kumulatívnych potvrdení:** Vďaka použitiu kumulatívneho potvrdzovania je protokol extrémne odolný voči strate samotných potvrdzovacích paketov. Ak sa na sieti stratí `DATA_ACK(2)`, ale klientovi neskôr dorazí `DATA_ACK(4)`, klient okamžite vie, že pakety 1 až 4 boli doručené v poriadku a posunie svoje okno.

## 6. Retransmisia a spracovanie timeoutov

Riadenie toku a detekcia strát sú implementované pomocou neblokujúceho systémového volania `poll()`. Protokol rozlišuje dve nezávislé vrstvy časovačov:

* **Lokálny časovač (Retransmisia):** Klient pri odoslaní dát čaká na potvrdenie (ACK) s krátkym vnútorným timeoutom. Ak potvrdenie pre najstarší neodoslaný paket (base) nedorazí včas, klient predpokladá, že sa paket (alebo jeho ACK) stratil na sieti, a v súlade s logikou Go-Back-N (GBN) odošle znova celé aktuálne okno.
* **Globálny časovač (Ukončenie pri zlyhaní):** Parametrom `-w` je definovaný kritický časový limit. Ide o čas, počas ktorého nenastane v protokole absolútne žiadny pokrok (neprídu žiadne nové dáta ani nové potvrdenia). Ak tento čas vyprší, sieť sa považuje za trvalo mŕtvu a aplikácia sa ukončí s chybovým kódom `TimeoutError (4)`.

### Zdôvodnenie návrhu (Design Decisions)
* **Prečo využiť `poll()`?** Použitie `poll()` umožňuje elegantne spojiť čakanie na dáta zo siete s presným meraním času v jednom vlákne. Vyhli sme sa tak zbytočne zložitému a na chyby náchylnému viacvláknovému programovaniu (multithreading) alebo používaniu asynchrónnych signálov.
* **Dôležitosť Globálneho timeoutu:** V reálnom svete sa môže stať, že sa odpojí kábel alebo spadne server. Bez globálneho timeoutu by sa klient donekonečna točil v slučke retransmisií (tzv. infinite hang). Tento mechanizmus zaručuje, že sa proces v OS po čase bezpečne ukončí a uvoľní porty.

---

## 7. Správa duplikátov a poškodených paketov

Protokol je navrhnutý tak, aby prežil aj vo vysoko chybovom prostredí (simulovanom napr. cez `tc netem`), pričom využíva nasledujúce mechanizmy:

* **Duplikáty:** Server si pamätá, ktoré sekvenčné číslo (`seq_num`) očakáva. Ak dorazí paket s číslom menším, ide o oneskorený duplikát. Server tieto dáta bezpečne zahodí (aby nepoškodil výstupný súbor), **ale okamžite odošle znova potvrdenie (ACK)** pre naposledy správne prijatý paket.
* **Pakety mimo poradia (Out-of-order):** Ak príde paket s číslom väčším, ako sa očakáva (napríklad prišiel paket 3, ale server ešte nedostal paket 2), server tento predčasný paket jednoducho zahodí.
* **Korupcia (Poškodenie):** Integrita každého paketu (hlavičky aj užitočných dát) je chránená 16-bitovým kontrolným súčtom (Internet Checksum podľa RFC 1071). Ak výpočet na strane prijímača nesedí s hodnotou v hlavičke, paket sa považuje za poškodený a je ticho zahodený (silent drop).

### Zdôvodnenie návrhu (Design Decisions)
* **Prečo odpovedať na duplikáty?** Je kriticky dôležité duplikáty len nezahodiť, ale na ne aj odpovedať. Ak by server mlčal, mohlo by dôjsť k uviaznutiu (deadlocku), kedy klient stále posiela dáta, ale nedostáva odpoveď, pretože predchádzajúci ACK sa stratil po ceste.
* **Prečo sa pakety mimo poradia zahadzujú?** Toto je štandardné správanie protokolu Go-Back-N. Hoci ukladanie paketov "do zásoby" (tzv. Selective Repeat) by ušetrilo nejakú šírku pásma, vyžadovalo by to komplexnú správu pamäťových bufferov na strane servera. GBN je jednoduchší, deterministickejší a menej náročný na operačnú pamäť.

---

## 8. Identifikácia pripojenia (Connection ID)

Na ochranu relácie a prenosu bol implementovaný mechanizmus multiplexingu pomocou `connection_id`. Každá nová relácia začína vygenerovaním náhodného 32-bitového kľúča (v móde klienta). Tento kľúč sa odsúhlasí počas inicializačného handshaku (`HELLO`). Následne musia všetky dátové aj potvrdzovacie pakety obsahovať toto presné ID.

### Zdôvodnenie návrhu (Design Decisions)
* **Ochrana pred "Alien" paketmi:** Keďže pracujeme nad bezstavovým UDP, operačný systém nerozlišuje, komu pakety patria. Ak by klient A stiahol súbor, ukončil sa, a hneď nato by začal klient B sťahovať iný súbor z toho istého portu, oneskorené pakety od klienta A (zaseknuté niekde na routroch v sieti) by mohli doraziť neskoro a prepísať dáta v súbore klienta B. Vďaka unikátnemu ID server tieto staré, zblúdené pakety (Alien packets) okamžite deteguje a bezpečne odignoruje.
* **Blokovanie súbežných spojení:** Ak sa počas prebiehajúceho prenosu pokúsi pripojiť iný klient s novým `HELLO` paketom, server ho vďaka nesúladu ID ignoruje, čím je splnená požiadavka zadania na obsluhu presne jedného prenosu.

````mermaid
classDiagram
    class Config {
        +bool is_server
        +bool is_client
        +string port_num
        +string address
        +string in_file
        +string out_file
        +int timeout
        +uint32_t connection_id
    }

    class Utils {
        <<Static>>
        +getHelp() void
        +calc_checksum(data, length) uint16_t
        +parse_args(argc, argv, config) int
    }

    class RdtServer {
        -int sockfd
        -Config config
        -pollfd pfd
        -ostream* out_stream
        -net_setup() void
        +RdtServer(cfg, out)
        +~RdtServer()
        +run() void
    }

    class RdtClient {
        -int sockfd
        -Config config
        -pollfd pfd
        -istream* in_stream
        -net_setup() void
        -send_hello(send_pkt, expected_flag) bool
        +RdtClient(cfg, in)
        +~RdtClient()
        +run() void
        +get_fd() int
        +get_addr_len() socklen_t
    }

    RdtServer o-- Config : Composition
    RdtClient o-- Config : Composition
````
````mermaid

sequenceDiagram
    participant C as RdtClient
    participant S as RdtServer

    Note over C,S: 1. Session Establishment (Handshake)
    C->>S: [HELLO] connection_id = X
    S-->>C: [HELLO_ACK] connection_id = X
    
    Note over C,S: 2. Data Transfer (Go-Back-N)
    C->>S: [DATA] seq_num = 1
    C->>S: [DATA] seq_num = 2
    S-->>C: [DATA_ACK] ack_num = 2
    
    Note over C,S: 3. Session Teardown
    C->>S: [FINAL] connection_id = X
    S-->>C: [FINAL_ACK] connection_id = X
    Note over C,S: Connection Closed safely
````
## 9. Testovanie

Testovanie bolo rozdelené do dvoch fáz a je plne zautomatizované pomocou príkazu `make test`. Testy prebehli na referenčnom `Nix` prostredí (`x86_64-linux`).

### A) C++ Unit Testy (`tests/test.cpp`)
* **Ako spustiť:** Vykonajú sa automaticky po zavolaní `make test`.
* **Čo bolo testované:**
  1. **CLI Parser:** Testovanie argumentov (napr. chýbajúci port, konflikt `-c` a `-s`, neplatný port `abc`). Očakávaný výstup: Návratový kód `1` (CliError). Skutočný výstup: Úspech, program bezpečne padá.
  2. **Checksum logic:** Vstupy `AABB` vs `AABBC` a overenie, či generujú správne a odlišné hashe podľa RFC 1071. Test na 0 bytov.
  3. **Network Mapping:** Testovanie `getaddrinfo` s platnými (`localhost`, `127.0.0.1`, `::1`) aj neplatnými adresami.

### B) Bash Integration Testz (`tests/test.sh`)
* **Ako spustiť:** Skript sa spúšťa automaticky na konci `make test`.
* **Prostredie:** Vyžaduje inštaláciu príkazu `tc` (Traffic Control) pre simuláciu chýb.
* **Testované scenáre a výsledky:**
  1. **Timeout Test:** Spustenie servera s `-w 2` bez klienta. 
     * *Očakávanie:* Server skončí po 2s s chybou. *Výsledok:* OK (Exit code != 0).
  2. **Alien Packet (Multiplexing):** Počas prenosu 5MB súboru sa pripojí druhý klient ("Hacker") s rovnakým portom, ale iným súborom. 
     * *Očakávanie:* Server odignoruje votrelca a prijme 5MB súbor bez poškodenia vďaka `connection_id`. *Výsledok:* OK (`cmp` nepotvrdil žiadnu korupciu).
  3. **I/O streamy:** Testovanie prenosu `Stdin -> Súbor`, a `Súbor -> Stdout` pomocou Linux rúry (`|`). *Výsledok:* OK.
  4. **Corner Cases:** Prenos súboru o veľkosti 0 bytov. *Výsledok:* OK, handshake a teardown prebehli bez pádu.
  5. **Network Hell:** Pomocou príkazu `sudo tc qdisc add dev lo root netem loss 10% duplicate 10%` bola vytvorená simulácia nekvalitnej siete. 
     * *Očakávanie:* Súbor musí byť prenesený bit-po-bite presne napriek strate a duplikácii, využitím Go-Back-N retransmisie. *Výsledok:* OK, súbor bol po prenose identický so zdrojovým (`cmp` = 0).

## 10. AI USAGE 
* **AI:** Použité pre štúdium a implementáciu funkcie `calc_checksum` na zaistenie integrity hlavičky a dát podľa 1071.
* **Učebné materiály IPK:** Slidy k prednáškam, hlavne (4., 5.) a referenčný repozitár fakulty (dev-envs).
* **AI Asistencia (Google Gemini):** LLM model bol počas vývoja použitý ako interaktívny konzultant. Špecifické využitie zahŕňalo:
  * Generovanie formátovania pre Mermaid UML grafy.
  * Pomoc pri písaní a štruktúrovaní komplexného Bash skriptu a Unit Testov.
  * Pomoc pri písaní dokumentácie a kontrola pravopisu.  
  * *Poznámka:* Samotná architektúra protokolu (Go-Back-N, zapuzdrenie, packet parsing) a kód v C++ boli navrhnuté a implementované autorom, pričom AI slúžila len na code-review a prípravu obhajoby.