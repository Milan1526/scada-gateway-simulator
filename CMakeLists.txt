# 1. Definisemo minimalnu verziju CMake-a koja je potrebna
cmake_minimum_required(VERSION 3.10)

# 2. Naziv celog projekta i jezik koji se koristi 
project(ScadaGateway CXX)

# 3. Zahtevamo standard C++ 17 kako bismo osigurali moderne funkcije
set(CMake_CXX_STANDARD 17)
set(CMake_CXX_STANDARD_REQUIRED ON)

# 4. Definisemo gde se nalaze nasi izvorni (.cpp) fajlovi
set(SOURCES
    src/main.cpp
)

# 5. Govorimo CMake-u da zelimo da napravimo izvrsni program (Executable)
# Prvi parametar je ime programa, drugi je lista fajlova
add_executable(scada_gateway ${SOURCES})

# 6. Automatsko uvezivanje mreznih i nitnih biblioteka u zavisnosti od OS-a 
# Threads (niti) su potrebne i na Windowsu i na Linuxu
find_package(Threads REQUIRED)
target_link_libraries(scada_gateway PRIVATE Threads::Threads)

# Ako kompajliramo na Windows-u (WIN32), moramo povezati sistemsku mreznu biblioteku ws2_32
if(WIN32)
    target_link_libraries(scada_gateway PRIVATE ws2_32)
endif()