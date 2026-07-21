#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>  // Biblioteka za upis i citanje fajlova 
#include <iomanip>  // Biblioteka za formatiranje ispisa (vreme, decimalni brojevi)
#include <ctime>    // Biblioteka za rad sa sistemskim vremenom
#include <atomic>
#include <csignal>
#include "NetworkServer.hpp"
#include "CircularBuffer.hpp"
#include "TelemetryPacket.hpp"

std::atomic<bool> running{true};
// Kruzni bafer sa 10 mesta.
CircularBuffer gateway_buffer(10);
// Pokrecemo server na portu 8080
NetworkServer server(8080);

// Pomocna funkcija koja vraca trenutno sistemsko vreme u tekstualnom formatu
std::string get_current_timestamp() {
    auto sada = std::chrono::system_clock::now();
    std::time_t vreme_t = std::chrono::system_clock::to_time_t(sada);
    std::tm lokalno_vreme;

    // Bezbedna funkcija za uzimanje lokalnog vremena na Windows-u
    localtime_s(&lokalno_vreme, &vreme_t);

    std::ostringstream oss;
    oss << std::put_time(&lokalno_vreme, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// Funkcija za upis alarma u CSV fajl
void log_alarm_to_csv(const TelemetryPacket& paket, const std::string& akcija) {
    // std::ios::app znaci APPEND - ne brise stari sadrzaj fajla, vec dodaje novi red
    std::ofstream csv_fajl("alarms.csv", std::ios::app);

    if (csv_fajl.is_open()) {
        // Ako je fajl prazan (tek napravljen), upisujemo zaglavlje (kolone)
        // Proveravamo velicinu fajla tako sto pomerimo kursor na kraj
        csv_fajl.seekp(0, std::ios::end);
        if (csv_fajl.tellp() == 0) {
            csv_fajl << "Timestamp,StationID,Voltage,Current,Frequency\n";
        }

        // Upisujemo podatke odvojene zarezom (CSV standard)
        csv_fajl << get_current_timestamp() << ","
                 << paket.station_id << ","
                 << std::fixed << std::setprecision(2) << paket.voltage << ","
                 << paket.current << ","
                 << paket.frequency << ","
                 << akcija << "\n";

        csv_fajl.close(); // Zatvaramo fajl i oslobadjamo resurse
        std::cout << "[LOGER] Alarm uspesno zapisan u alarms.csv" << std::endl;
    }
}
// Globalna funkcija koja hvata operativni sistem za gasenje (CTRL + C)
void signal_handler(int signal_num) {
    if (signal_num == SIGINT) {
        std::cout << "\n[SISTEM] Uhvacen signal za gasenje (CTRL + C). Pokrecem Graceful shutdown" << std::endl;
        running.store(false);  // Obavestavamo sve niti da prekinu rad
        server.stop();  // Prekidamo blokirajuci mrezni recv()
    } 
}

// Funkcija koju izvrsava pozadinska nit (mrezna nit)
void network_received_thread(NetworkServer& server_ref) {
    std::cout << "[NIT-MREZA] Pokrenuta nit za neprekidan prijem podataka" << std::endl;

    // Beskonacna petlja server stalno slusa mrezu i prima pakete jedan za drugim
    while (running.load()) {
        TelemetryPacket lokalni_paket;

        // Prosledjujemo atomic indikator unutra da mrezna logika zna ako treba da stane  
        if (server_ref.receive_packet(lokalni_paket, running)) {
            std::cout << "[Mrezna nit] Primljen paket od stanice " << lokalni_paket.station_id <<
            " Guram paket u kruzni bafer" << std::endl;

            // Bezbedno ubacivanje u bafer (Mutex odradjuje zakljucavanje)
            gateway_buffer.push(lokalni_paket);
        } else {
            // Ako je veza pukla, prekidamo petlju i gasimo nit
            std::cout << "[Mrezna nit] Greska ili prekid veze na mrezi. Gasim nit" << std::endl;
            break;
        }
    }
    std::cout << "[Mrezna nit] Nit bezbedno ugasena" << std::endl;
}

int main() {
    std::cout << "=== SCADA GATEWAY SA SMART TRIP LOGIKOM" << std::endl;

    // Registrujemo ruku operativnog sistema koja hvata prekid rada 
    std::signal(SIGINT, signal_handler);

    // start() Blokira glavni program dok se Python simulator ne poveze 
    if (!server.start()) {
        std::cerr << "[Glavna nit] Neuspesno podizanje mreznog servera" << std::endl;
        return 1;
    }

    // Kada se Python uspesno poveze, pokrecemo pozadinsku nit za prijem podataka
    // Prosledjujemo server kao referencu (std::ref) da nit koristi isti mrezni objekat
    std::thread mrezna_nit(network_received_thread, std::ref(server));
    std::cout << "[Glavna nit] Ulazim u petlju za obradu podataka (Analizator)" << std::endl;

    // Globalna petlja analizatora radi dok god je running true ili dok ima neobradjenih paketa u baferu
    while (running.load() || gateway_buffer.size() > 0) {
        // Ako u baferu ima podataka, obradi ih)
        if (gateway_buffer.size() > 0) {
            TelemetryPacket paket = gateway_buffer.pop();

            std::cout << "[Analizator] Obrada -> Stanica: " << paket.station_id << 
            "Napon: " << paket.voltage << " V" << std::endl;

            // Logika za aktiviranje alarma
            if (paket.voltage > 230.0f) {
                std::cerr << "[ALARM] Visok napon na stanici: " << paket.station_id << std::endl;

                // Formatiramo komandu za gasenje u tekstualnom formatu (npr. "TRIP:10\n")
                std::string trip_komanda = "TRIP:" + std::to_string(paket.station_id) + "\n";

                // Saljemo povratnu komandu klijentu nazad kroz soket
                server.send_response(trip_komanda);

                // Upisujemo ovaj incident u trajni csv fajl
                log_alarm_to_csv(paket, "AUTOMATIC_TRIP_TRIGGERED");
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    std::cout << "[Glavna nit] Ciscenje i zatvaranje niti" << std::endl;
    if (mrezna_nit.joinable()) {
        mrezna_nit.join();
    }

    std::cout << "[Sistem] SCADA GATEWAY je uspesno ugasen i bezbedan. Prijatno!" << std::endl;
    return 0;
}
