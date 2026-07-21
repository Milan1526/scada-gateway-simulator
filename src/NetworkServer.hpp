#pragma once
#include <iostream>
#include <atomic>
#include <winsock2.h> // glavna windows mrezna biblioteka
#include "TelemetryPacket.hpp"

// Da bi radilo na windowsu, moramo reci kompajleru da poveze mreznu sistemsku biblioteku ws2_32.lib
#pragma comment(lib, "ws2_32.lib")

class NetworkServer {
private: 
    SOCKET server_socket;
    SOCKET client_socket;
    int port;
public: 
    NetworkServer(int server_port) {
        port = server_port;
        server_socket = INVALID_SOCKET;
        client_socket = INVALID_SOCKET;
    }

    // Funkcija koja pokrece mreznu kapiju 
    bool start() {
        WSADATA wsaData;
        // Korak 0: Inicijalizacija Windows mreznog podsistema (specificno samo za Windows)
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) !=0) {
            std::cerr << "[GRESKA] Inicijalizacija Winsocka neuspesna! " << std::endl;
            return false;
        }

        // Korak 1: kreiranje soketa (Otvoranje telefonske linije)
        // AF_INET znaci IPv4 adrese, SOCK_STREAM znaci TCP protokol
        server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server_socket == INVALID_SOCKET) {
            std::cerr << "[GRESKA] Kreiranje soketa neuspesno! " << std::endl;
            WSACleanup();
            return false;
        }

        // Oslobadjanje porta odmah nakon gasenja
        char value = 1;
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

        // Podesavanje adrese i porta na kojem ce server slusati
        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;  // slusaj na svim dostupnim mreznim karticama (127.0.0.1)
        server_addr.sin_port = htons(port);  // htons pretvara broj porta u broj koji mreza razume

        // Korak 2: Vezivanje (Bind) - Zakupljujemo port (8080)
        if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::cerr << "[GRESKA] Bind (vezivanje za port) neuspesno! " << std::endl;
            closesocket(server_socket);
            WSACleanup();
            return false;
        }

        // Korak 3: Slusanje (Listen) - Podizemo slusalicu i cekamo simulator da "okrene broj"
        // broj 3 znaci da maksimalno 3 veze mogu da cekaju u redu pre nego sto ih prihvatimo
        if (listen(server_socket, 3) == SOCKET_ERROR) {
            std::cerr << "[GRESKA] Slusanje na portu neuspesno! " << std::endl;
            closesocket(server_socket);
            WSACleanup();
            return false;
        }

        std::cout << "[SERVER] Mrezna kapija uspesno podignuta na portu " << port << "Cekam povezivanje" << std::endl;

        // Korak 4: Prihvatanje veze (Accept) - Program ovde staje i ceka dok se Python simulator ne javi
        client_socket = accept(server_socket, nullptr, nullptr);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "[GRESKA] Prihvatanje veze neuspesno! " << std::endl;
            closesocket(server_socket);
            WSACleanup();
            return false;
        }

        std::cout << "[SERVER] Python simulator se uspesno povezao preko TCP-A" << std::endl;
        return true;
    } 

    // NOVA FUNKCIJA: Robustan prijem koji garantuje tacno 24 bajta uz proveru stanja aplikacije
    bool receive_packet(TelemetryPacket& izlazni_paket, const std::atomic<bool>& global_running) {
        // Pravimo privremeni bafer od 24 bajta za sirove podatke
        char mrezni_bafer[24]; 
        int ukupno_primljeno = 0;

        while (ukupno_primljeno < 24) {
            // Ako je pokrenuto gasenje spolja, prekidamo mrezni prijem
            if (!global_running.load()) {
                return false;
            }

            int preostalo = 24 - ukupno_primljeno;
            int primljeno_sada = recv(client_socket, mrezni_bafer + ukupno_primljeno, preostalo, 0);

            if (primljeno_sada == 0) {
                // Klijent je zatvorio vezu
                return false;
            }

            ukupno_primljeno += primljeno_sada;

        }

        memcpy(&izlazni_paket, mrezni_bafer, 24);
        return true;
    }

    void stop() {
        if (client_socket != INVALID_SOCKET) {
            shutdown(client_socket, SD_BOTH);
            closesocket(client_socket);
            client_socket = INVALID_SOCKET;
        }
        if (server_socket != INVALID_SOCKET) {
            closesocket(server_socket);
            server_socket = INVALID_SOCKET;
        }
        WSACleanup();
    }

    // Nova funkcija: slanje povratne komande kroz klijentski soket nazad klijentu
    bool send_response(const std::string& komanda) {
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "[GRESKA] Slanje neuspesno: Klijent povezan!" << std::endl;
            return false;
        }

        // koristimo winsock funkciju u send()
        // c_str() pretvara C++ string u sirovi char pointer koji mreza trazi
        int poslato_bajtova = send(client_socket, komanda.c_str(), komanda.length(), 0);

        if (poslato_bajtova == SOCKET_ERROR) {
            std::cerr << "[GRESKA] Slanje komande " << komanda << " neuspesno!" << std::endl;
            return false;
        }

        std::cout << "[SERVER-MREZA] Uspesno poslata komanda nazad klijentu: " << komanda << std::endl;
        return true;
    }

    // Destruktor: Zatvaramo mrezne veze kada se program gasi
    ~NetworkServer() {
        stop();
    }
};
