import socket 
import time 
import struct
import threading
from http.server import HTTPServer, BaseHTTPRequestHandler
import json

# Globalna memorija u Python-u koja pamti istoriju poslatih podataka za potrebe grafikona
podaci_za_web = []
# Recnik koji prati stanje osiguraca za svaku sitnicu (True = U redu, False = Izbio/Iskljucen)
stanje_osiguraca = {10: True, 11: True, 12: True} 

# Klasa koja upravlja zahtevima iz browsera
class DashboardHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        global podaci_za_web

        if self.path == '/':
            # Servisiramo glavnu HTML stranicu sa dashborard-om
            self.send_response(200)
            self.send_header('Content-type', 'text/html; charset=utf-8')
            self.end_headers()

            html_kod = """
            <!DOCTYPE html>
            <html lang="sr">
            <head>
                <meta charset="UTF-8">
                <title>SCADA Industrijski Dashboard</title>
                <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
                <style>
                    body { background-color: #121212; color: #e0e0e0; font-family: 'Segoe UI', sans-serif; margin: 30px; }
                    h1 { color: #00adb5; border-bottom: 2px solid #393e46; padding-bottom: 10px; }
                    .container { display: flex; gap: 20px; flex-wrap: wrap; }
                    .card { background-color: #1e1e1e; padding: 20px; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); flex: 1; min-width: 45%; }
                    table { width: 100%; border-collapse: collapse; margin-top: 15px; }
                    th, td { padding: 12px; text-align: left; border-bottom: 1px solid #393e46; }
                    th { background-color: #00adb5; color: #fff; }
                    tr:hover { background-color: #252525; }
                    .alarm { color: #ff2e63; font-weight: bold; }
                    .tripped { color: #ff9f43; font-weight: bold; font-style: italic; }
                </style>
            </head>
            <body>
                <h1>🏭 SCADA Energetski Monitor — Trip Logic Dashboard</h1>
                <div class="container">
                    <div class="card"><canvas id="naponChart"></canvas></div>
                    <div class="card">
                        <h3>Merenja i Stanje Prekidača</h3>
                        <table>
                            <thead>
                                <tr><th>Stanica ID</th><th>Napon</th><th>Struja</th><th>Status Sistema</th></tr>
                            </thead>
                            <tbody id="tabela-podaci"></tbody>
                        </table>
                    </div>
                </div>
                <script>
                    const ctx = document.getElementById('naponChart').getContext('2d');
                    const naponChart = new Chart(ctx, {
                        type: 'line',
                        data: { labels: [], datasets: [{ label: 'Napon (V)', data: [], borderColor: '#00adb5', borderWidth: 2 }] },
                        options: { scales: { y: { min: 0, max: 250 } } }
                    });

                    async function osveziPodatke() {
                        const odgovor = await fetch('/api/data');
                        const podaci = await odgovor.json();
                        const tabelaBody = document.getElementById('tabela-podaci');
                        tabelaBody.innerHTML = '';
                        naponChart.data.labels = [];
                        naponChart.data.datasets[0].data = [];

                        podaci.forEach((p, index) => {
                            let statusText = p.status === "TRIPPED" ? '<span class="tripped">[OSIGURAČ IZBIO]</span>' : '<span style="color:#00e676">Normalno</span>';
                            if(p.voltage > 230 && p.status !== "TRIPPED") statusText = '<span class="alarm">[KRITIČNO]</span>';

                            const red = `<tr>
                                <td>${p.station_id}</td>
                                <td>${p.voltage.toFixed(1)} V</td>
                                <td>${p.current.toFixed(1)} A</td>
                                <td>${statusText}</td>
                            </tr>`;
                            tabelaBody.innerHTML += red;
                            naponChart.data.labels.push(`M ${index + 1}`);
                            naponChart.data.datasets[0].data.push(p.voltage);
                        });
                        naponChart.update();
                    }
                    setInterval(osveziPodatke, 1000);
                </script>
            </body>
            </html>
            """
            self.wfile.write(html_kod.encode('utf-8'))
        
        elif self.path == '/api/data':
            # JSON API endpoint sa koga nas JavaScript cita podatke
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps(podaci_za_web).encode('utf-8'))

    # Onemogucavamo dosadne logove u konzoli za svaki HTTP zahtev
    def log_message(self, format, *args):
        return

# Funkcija koja podize web server u pozadinskoj niti
def pokreni_web_server():
    server_address = ('', 8000)
    httpd = HTTPServer(server_address, DashboardHandler)
    print("[WEB SERVER] Dashboard je dostupan na adresi: http://localhost:8000")
    httpd.serve_forever()

# Pozadinska nit koja ceka komande za gasenje osiguraca iz C++ 
def osluskuj_feedback_iz_cpp(sock):
    global stanje_osiguraca
    while True:
        try:
            odgovor = sock.recv(1024).decode('utf-8')
            if not odgovor:
                break
            if "TRIP:" in odgovor:
                # Izvlacimo ID stanice iz poruke "TRIP:10\n"
                station_id = int(odgovor.strip().split(":")[1])
                print(f"\n⚠️ [PYTHON] Primljen signal za havariju od C++! Izbija osigurac na stanici {station_id}!")
                stanje_osiguraca[station_id] = False
        except:
            break

def main():
    print("=== Python Simulator: Posiljalac, Vizuelizator i  ===") 


    # Pokrecemo Web Dashboard u zasebnoj niti pre slanja na C++ mrezni soket
    global podaci_za_web, stanje_osiguraca
    threading.Thread(target=pokreni_web_server, daemon=True).start()

    # IP adresa naseg racunara (localhost) i port na kom slusa C++ server 
    host = "127.0.0.1" 
    port = 8080

    # Generisanje paketa za simulaciju u realnom vremenu
    paketi = [
        (10, 1718912401, 220.5, 12.1, 50.0),
        (10, 1718912402, 224.2, 13.0, 50.0),
        (10, 1718912403, 235.8, 14.5, 50.1), # ALARM
        (10, 1718912404, 225.0, 12.0, 50.0), 

        (11, 1718912404, 219.2, 10.0, 49.9),
        (11, 1718912405, 221.0, 11.2, 50.0),

        (12, 1718912406, 238.1, 15.2, 50.2), # ALARM
        (12, 1718912407, 225.0, 12.0, 50.0)
    ]

    try:
        # Kreiramo socket (AF_INET = IPv4, SOCK_STREAM = TCP)
        client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        # Povezujemo se na server 
        client_socket.connect((host, port))
        print("[KLIJENT] Uspesno povezan na C++ server, saljem telemetriju")

        # Pokrecemo nit koja ce non stop da slusa sta C++ vraca nazad
        threading.Thread(target=osluskuj_feedback_iz_cpp, args=(client_socket,), daemon=True).start()

        for p in paketi:
            s_id = p[0]

            # Industrijska logika: ako je osigurac za ovu stanicu izbio, prekidamo slanje realnih vrednosti!
            if not stanje_osiguraca[s_id]:
                print(f"[SIMULATOR] Stanica {s_id}, je iskljucena (Osigurac izbijen). Saljem bezbednosnu nulu.")
                mrezni_paket = struct.pack("<IQfff", s_id, int(time.time()), 0.0, 0.0, 0.0)
                client_socket.sendall(mrezni_paket)
                podaci_za_web.append({"station_id": s_id, "voltage": 0.0, "current": 0.0, "frequency": 0.0, "status": "TRIPPED"})
            else:
                mrezni_paket = struct.pack("<IQfff", *p)
                client_socket.sendall(mrezni_paket)
                print(f"[SIMULATOR] Saljem redovna merenja za Stanicu: {s_id}")
                podaci_za_web.append({"station_id": s_id, "voltage": p[2], "current": p[3], "frequency": p[4], "status": "OK"})

            if len(podaci_za_web) > 10: podaci_za_web.pop(0)
            time.sleep(2.0)

        # Zatvaramo vezu nakon uspesnog povezivanja
        client_socket.close()
        print("[KLIJENT] Veza sa serverom zatvorena!")

        print("[SISTEM] Web Dashboard ostaje aktivan. Pritisni CTRL+C za gasenje")
        while True:
            time.sleep(1)

    except Exception as e:
        print(f"[GRESKA] Povezivanje neuspesno: {e}")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n[SISTEM] Uspesno i bezbedno ugasen ceo SCADA simulator, pozdrav")
