# SCADA Energetski Gateway & Real-Time Monitor

![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat&logo=c%2B%2B)
![Python](https://img.shields.io/badge/Python-3.x-3776AB?style=flat&logo=python)
![CMake](https://img.shields.io/badge/Build-CMake-064F8C?style=flat&logo=cmake)
![Architecture](https://img.shields.io/badge/Architecture-Multithreaded-brightgreen)

Vizuelizacija i upravljački sistem u realnom vremenu namenjen za nadzor energetskih podstanica. Projekat simulira komunikaciju sa senzorskim jedinicama preko TCP/IP protokola, vrši bezbednu obradu podataka kroz kružni bafer u višenitnom okruženju, pruža automatsku zaštitu mreže (**Closed-Loop Trip Logic**) i prikazuje žive metrike na web-baziranom dashboard-u.

---

## Arhitektura Sistema

Sistem je koncipiran kao distribuirani mrežni gateway sa dvosmernom komunikacijom:

```text
  [ Python Simulator ]  <--- (TCP Feedback: TRIP Signal) ---  [ C++ SCADA Gateway ]
  (Predajnik telemetrije) --- (TCP Data: Custom Binary) --->  (Server / Analizator)
          |                                                            |
          v                                                            v
  [ Web Dashboard ]                                           [ CSV Loger Incidenta ]
(HTTP Dashboard @ 8000)                                       (alarms.csv Audit Log)

1. Ključne komponente:
C++ Core Engine (Inženjerski sloj):

Thread-Safe Circular Buffer: Pravi kružni bafer implementiran uz std::mutex i std::lock_guard za siguran prenos podataka između niti.
Mrežni Server (Winsock2 / POSIX Sockets): TCP server koji raspakuje prilagođene binarne pakete (TelemetryPacket).
Graceful Shutdown: Bezbedno gašenje svih niti i oslobađanje mrežnih resursa hvatanjem SIGINT (CTRL+C) signala.
Automatska Trip Logika: Detekcija prenapona (>230V) sa trenutnim povratnim slanjem komande za isključenje prekidača (Feedback Control).
Python Simulator & Web Visualizer:
Multi-threaded Client: Simulator koji šalje binarno spakovane pakete pomoću struct modula i istovremeno u pozadinskoj niti sluša povratne komande iz C++ servera.
Web Dashboard: HTTP server sa Chart.js grafikonom koji osvežava stanje napona, struje i status osigurača u realnom vremenu.

2. Struktura Projekta

moj_scada_projekat/
├── CMakeLists.txt         # Kros-platformska konfiguracija za kompajliranje
├── src/                   # Izvorni C++ kod
│   ├── main.cpp           # Glavna petlja analizatora i mrežna nit
│   ├── NetworkServer.hpp  # Implementacija TCP socket servera
│   ├── CircularBuffer.hpp # Nitno-bezbedan kružni bafer
│   └── TelemetryPacket.hpp# Binarna struktura telemetrijskog paketa
├── scripts/               # Pomoćne Python skripte
│   └── test-client.py     # Simulator telemetrije i Web Dashboard
└── build/                 # Direktorijum za generisanje binarnih fajlova

3. Uputstvo za Kompajliranje i Pokretanje
Preduslovi:
C++ Compiler (GCC, Clang, ili MSVC sa podrškom za C++17)
CMake (verzija 3.10 ili novija)
Python 3.x

3.1 Kompajliranje C++ Gateway-a
Uđe se u koren projekta i pokrene standardni CMake algoritam:

mkdir build
cd build
cmake ..
cmake --build .

3.2 Pokretanje Sistema
3.2.1 Pokretanje C++ Servera:
Unutar build/ direktorijuma pokrenuti generisani binarni fajl:

./scada_gateway.exe

3.2.2 Pokretanje Python Simulatora:
U drugom terminalu (iz korena projekta) pokrenuti skriptu:

python scripts/scripts/test-client.py   # ili python scripts/test-client.py

4. Pratite Web Dashboard:
Otvorite vaš browser i idite na adresu: http://localhost:8000

5. Bezbednosna Trip Logika u Radu

Kada napon na bilo kojoj stanici pređe vrednost od 230.0 V:
C++ Analizator presreće paket i trenutno zapisuje incident u alarms.csv.
C++ Server šalje povratni niz "TRIP:<station_id>" nazad kroz otvoreni TCP socket.
Python simulator u svojoj pozadinskoj niti presreće poruku, aktivira zaštitni relej i spušta merenja za tu stanicu na 0.0 V.
Web Dashboard automatski prikazuje status [OSIGURAČ IZBIO] i pad grafikona na nulu.
