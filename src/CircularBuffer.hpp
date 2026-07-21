#pragma once
#include <vector>
#include <iostream>
#include <mutex>
#include "TelemetryPacket.hpp"

class CircularBuffer {
private:
    std::vector<TelemetryPacket> buffer;  // Fiksni niz paketa u memoriji
    size_t capacity; // Maksimalna velicina bafera
    size_t head;  // Pozicija za pisanje (Glava)
    size_t tail;  // Pozicija za citanje (Rep)
    size_t current_size; // Dodajemo brojac da lakse znamo koliko trenutno imamo paketa 

    std::mutex mtx;  // ovo je objekat softverske bravice koji ce zakljucavati niti

public:
    // Konstruktor: Funkcija koja se pokrece kada stvaramo bafer i postavlja mu velicinu
    CircularBuffer(size_t max_size) {
        capacity = max_size;
        buffer.resize(capacity);   // Rezervisemo tacan broj mesta u memoriji
        head = 0;
        tail = 0;
        current_size = 0;
    }

    // Funkcija za dodavanje paketa (Mrezna nit donosi podatak)
    void push(TelemetryPacket packet) {
        //lock_guard automatski zakljucava mutex na pocetku funkcije,
        // a otklucava ga cim se funkcija zavrsi
        std::lock_guard<std::mutex> lock(mtx);

        // Korak 2: Upisujemo paket na mesto gde pokazuje glava (head)
        buffer[head] = packet;

        // Korak 3: Pomeramo head unapred, ali uz upotrebu % (modulo) da se vrati na 0 ako predje granicu
        head = (head + 1) % capacity;

        // Korak 4: Povecavamo trenutno broj paketa u baferu
        if (current_size < capacity) {
            current_size++;
        } else {
            // Ako je bafer bio skroz pun, head je upravo prepisao najstariji paket.
            // U tom slucaju, moramo i rep (tail) pomeriti za jedno mestu unapred da ne citamo smece.
            tail = (tail + 1) % capacity;
            std::cout << "[UPOZORENJE] Bafer je pun! Prepisujem najstariji podatak." << std::endl;
        }
    }

    // Funkcija za uzimanje paketa (Analizator uzima podatak)
    TelemetryPacket pop() {
        std::lock_guard<std::mutex> lock(mtx);
        if (current_size == 0) {
            // Ako je bafer prazan, vracamo prazan paket
            return TelemetryPacket{0, 0, 0.0f, 0.0f, 0.0f};
        }

        // Korak 1: Uzimamo paket sa mesta gde pokazuje Rep (tail)
        TelemetryPacket packet = buffer[tail];

        // Korak 2: Pomeramo tail unapred pomocu modulo operacije
        tail = (tail + 1) % capacity;

        // Korak 3: Smanjujemo broj paketa u baferu
        current_size--;

        // Korak 4: Otkljucavamo bravicu funkcije

        return packet;
    }

    // Pomocna funkcija da spoljni svet moze proveriti koliko ima paketa
    size_t size()  {
        std::lock_guard<std::mutex> lock(mtx);
        return current_size;
    }
};